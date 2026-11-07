// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PlatformSession.h"

#include <yyjson.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "HttpClient.h"
#include "src/helpers/Json.h"
#include "src/helpers/SecretStore.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"

namespace net {

namespace {

constexpr std::string_view clientId = "soundbuddy";

// renewed that long before it ends, not to send what expires on the way
constexpr std::int64_t expiryMarginSeconds = 60;

std::int64_t nowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// as application/x-www-form-urlencoded takes it
std::string urlEncoded(std::string_view text) {
    std::string encoded;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        const auto unreserved = (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') || c == '-' ||
                                c == '.' || c == '_' || c == '~';
        encoded += unreserved ? std::string(1, c) : std::format("%{:02X}", byte);
    }
    return encoded;
}

std::string field(std::string_view name, std::string_view value) {
    return std::format("{}={}", name, urlEncoded(value));
}

// answers of the token endpoint that mean these credentials will not work : others may be worth trying again
bool isRefusal(const Response& response) {
    return response.error.empty() && (response.status == 400 || response.status == 401);
}

Response refused(std::string why) {
    Response response;
    response.status = 403;
    response.body = std::move(why);
    return response;
}

}  // namespace

std::string PlatformSession::apiUrlOf(std::string_view hostUrl) {
    auto url = std::string { hostUrl };
    while (url.ends_with('/')) url.pop_back();

    const auto schemeEnd = url.find("://");
    const auto address = schemeEnd == std::string::npos ? url : url.substr(schemeEnd + 3);
    const auto host = address.substr(0, address.find_first_of(":/"));

    const auto wantsUnsecure = host == "localhost" || host == "127.0.0.1";
    return (wantsUnsecure ? "http://" : "https://") + address;
}

std::string PlatformSession::explain(const Response& response) {
    const auto doc = json::parse(response.body);
    auto* const root = json::root(doc);
    if (yyjson_is_obj(root)) {
        for (const char* key : { "error_description", "detail" }) {
            const auto said = json::string(yyjson_obj_get(root, key));
            if (!said.empty()) return std::string { said };
        }
    }
    return response.body;
}

PlatformSession::PlatformSession(
    const std::string& hostUrl, std::string username, std::filesystem::path tokensFile, std::string password,
    std::function<void()> onPasswordUsed
) :
    _hostUrl(hostUrl),
    _apiUrl(apiUrlOf(hostUrl)),
    _username(std::move(username)),
    _tokensFile(std::move(tokensFile)),
    _onPasswordUsed(std::move(onPasswordUsed)),
    _password(std::move(password)) {
    if (_password.empty()) {
        _loadTokens();
    } else {
        _forgetTokens();
    }
}

bool PlatformSession::hasLogin() const {
    const std::scoped_lock lock { _mutex };
    return !_password.empty() || !_refreshToken.empty();
}

Response PlatformSession::send(HttpClient& client, const Request& request, const std::stop_token& stop) {
    std::string tokenRefused;

    // once again if the token is refused, in case it just ended
    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto access = _access(client, stop, tokenRefused);
        if (access.token.empty()) return access.failure;

        auto authorized = request;
        authorized.headers.push_back({ .name = "Authorization", .value = "Bearer " + access.token });

        auto response = client.send(authorized, stop);
        if (response.status != 401 || attempt == 1) return response;
        tokenRefused = access.token;
    }

    return {};
}

PlatformSession::Access PlatformSession::_access(HttpClient& client, const std::stop_token& stop, const std::string& tokenRefused) {
    std::unique_lock lock { _mutex };

    if (!_exchange.waitForTurn(lock, stop)) {
        Response cancelled;
        cancelled.error = "Request cancelled";
        return { .failure = std::move(cancelled) };
    }

    if (_password.empty()) {
        if (_refreshToken.empty()) return { .failure = refused(std::string { i18n::tr("Waiting for appropriate credentials.") }) };

        // another thread may have renewed it meanwhile
        const auto stillGood = _accessToken != tokenRefused && nowSeconds() < _accessExpiresAt - expiryMarginSeconds;
        if (stillGood) return { .token = _accessToken };
    }

    return _exchange.fly(lock, [this, &client, &stop, password = _password, refreshToken = _refreshToken] {
        return _exchangeTokens(client, stop, password, refreshToken);
    });
}

PlatformSession::Access PlatformSession::_exchangeTokens(
    HttpClient& client, const std::stop_token& stop, const std::string& password, const std::string& refreshToken
) {
    if (!password.empty()) {
        auto response = _askTokens(
            client, stop,
            std::format(
                "{}&{}&{}&{}", field("grant_type", "password"), field("client_id", clientId), field("username", _username),
                field("password", password)
            )
        );
        if (!response.ok() && !isRefusal(response)) return { .failure = std::move(response) };

        {
            const std::scoped_lock lock { _mutex };
            _password.clear();
        }
        if (_onPasswordUsed) _onPasswordUsed();
        if (!response.ok()) return { .failure = refused(explain(response)) };

        const std::scoped_lock lock { _mutex };
        return { .token = _accessToken };
    }

    auto response = _askTokens(
        client, stop,
        std::format("{}&{}&{}", field("grant_type", "refresh_token"), field("client_id", clientId), field("refresh_token", refreshToken))
    );
    if (response.ok()) {
        const std::scoped_lock lock { _mutex };
        return { .token = _accessToken };
    }
    if (!isRefusal(response)) return { .failure = std::move(response) };

    logs::warning("Session", "The platform ended this login : {}", explain(response));
    _forgetTokens();
    return { .failure = refused(explain(response)) };
}

Response PlatformSession::_askTokens(HttpClient& client, const std::stop_token& stop, std::string_view fields) {
    Request request { .method = "POST", .url = _apiUrl + "/oauth/token" };
    request.headers = {
        { .name = "Content-Type", .value = "application/x-www-form-urlencoded" },
        { .name = "Accept-Language", .value = std::string { i18n::languageCode() } },
    };
    request.body = std::string { fields };

    auto response = client.send(request, stop);
    if (response.ok()) _keepTokens(response);
    return response;
}

void PlatformSession::_keepTokens(const Response& response) {
    const auto doc = json::parse(response.body);
    auto* const root = json::root(doc);

    const std::string access { json::string(yyjson_obj_get(root, "access_token")) };
    const std::string refresh { json::string(yyjson_obj_get(root, "refresh_token")) };
    const auto expiresAt = nowSeconds() + json::integer(yyjson_obj_get(root, "expires_in"));

    {
        const std::scoped_lock lock { _mutex };
        _accessToken = access;
        _refreshToken = refresh;
        _accessExpiresAt = expiresAt;
    }

    const auto stored = json::create();
    auto* const object = yyjson_mut_obj(stored.get());
    yyjson_mut_doc_set_root(stored.get(), object);
    yyjson_mut_obj_add_strncpy(stored.get(), object, "host", _hostUrl.data(), _hostUrl.size());
    yyjson_mut_obj_add_strncpy(stored.get(), object, "username", _username.data(), _username.size());
    yyjson_mut_obj_add_strncpy(stored.get(), object, "access", access.data(), access.size());
    yyjson_mut_obj_add_strncpy(stored.get(), object, "refresh", refresh.data(), refresh.size());
    yyjson_mut_obj_add_sint(stored.get(), object, "accessExpiresAt", expiresAt);

    // works until the next start : the login is then asked again
    if (!secrets::store(_tokensFile, json::write(stored))) logs::warning("Session", "The login could not be stored");
}

void PlatformSession::_forgetTokens() {
    {
        const std::scoped_lock lock { _mutex };
        _accessToken.clear();
        _refreshToken.clear();
        _accessExpiresAt = 0;
    }
    secrets::remove(_tokensFile);
}

void PlatformSession::_loadTokens() {
    const auto content = secrets::load(_tokensFile);
    if (!content) return;

    const auto doc = json::parse(*content);
    auto* const root = json::root(doc);
    if (json::string(yyjson_obj_get(root, "host")) != _hostUrl || json::string(yyjson_obj_get(root, "username")) != _username) return;

    _accessToken = json::string(yyjson_obj_get(root, "access"));
    _refreshToken = json::string(yyjson_obj_get(root, "refresh"));
    _accessExpiresAt = json::integer(yyjson_obj_get(root, "accessExpiresAt"));
}

}  // namespace net

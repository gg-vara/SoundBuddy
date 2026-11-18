// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PlatformSession.h"

#include <yyjson.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
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

// what SoundBuddy asks to do, which the user allows in the browser : offline_access, to stay logged in with a refresh token
constexpr std::string_view scopes = "library:write shout:write offline_access";

// OAuth's endpoints, as Better Auth serves them on the platform
constexpr std::string_view authorizeEndpoint = "/api/auth/oauth2/authorize";
constexpr std::string_view tokenEndpoint = "/api/auth/oauth2/token";
constexpr std::string_view revokeEndpoint = "/api/auth/oauth2/revoke";

// renewed that long before it ends, not to send what expires on the way
constexpr std::int64_t expiryMarginSeconds = 60;

// what is waiting for tokens is told sooner than the default
constexpr std::chrono::seconds tokensTimeout { 10 };

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

PlatformSession::PlatformSession(const std::string& hostUrl, std::filesystem::path tokensFile) :
    _hostUrl(hostUrl),
    _apiUrl(apiUrlOf(hostUrl)),
    _tokensFile(std::move(tokensFile)) {
    _loadTokens();
}

bool PlatformSession::hasLogin() const {
    const std::scoped_lock lock { _mutex };
    return !_refreshToken.empty();
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

    if (_refreshToken.empty()) return { .failure = refused(std::string { i18n::tr("Waiting for appropriate credentials.") }) };

    // another thread may have renewed it meanwhile
    const auto stillGood = _accessToken != tokenRefused && nowSeconds() < _accessExpiresAt - expiryMarginSeconds;
    if (stillGood) return { .token = _accessToken };

    return _exchange.fly(lock, [this, &client, &stop, refreshToken = _refreshToken] { return _refresh(client, stop, refreshToken); });
}

PlatformSession::Access PlatformSession::_refresh(HttpClient& client, const std::stop_token& stop, const std::string& refreshToken) {
    auto response = _post(
        client, stop, tokenEndpoint,
        std::format(
            "{}&{}&{}&{}", field("grant_type", "refresh_token"), field("client_id", clientId), field("refresh_token", refreshToken),
            field("resource", resource())
        )
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

std::string PlatformSession::authorizationUrl(std::string_view redirectUri, std::string_view state, std::string_view challenge) const {
    return std::format(
        "{}{}?{}&{}&{}&{}&{}&{}&{}&{}", _apiUrl, authorizeEndpoint, field("response_type", "code"), field("client_id", clientId),
        field("redirect_uri", redirectUri), field("scope", scopes), field("state", state), field("code_challenge", challenge),
        field("code_challenge_method", "S256"), field("resource", resource())
    );
}

Response PlatformSession::login(
    HttpClient& client, std::string_view code, std::string_view verifier, std::string_view redirectUri, const std::stop_token& stop
) {
    auto response = _post(
        client, stop, tokenEndpoint,
        std::format(
            "{}&{}&{}&{}&{}&{}", field("grant_type", "authorization_code"), field("client_id", clientId), field("code", code),
            field("code_verifier", verifier), field("redirect_uri", redirectUri), field("resource", resource())
        )
    );
    if (isRefusal(response)) return refused(explain(response));
    return response;
}

void PlatformSession::logout(HttpClient& client, const std::stop_token& stop) {
    std::string refreshToken;
    {
        const std::scoped_lock lock { _mutex };
        refreshToken = _refreshToken;
    }
    _forgetTokens();
    if (refreshToken.empty()) return;

    // its answer changes nothing here : the login is over for SoundBuddy anyway
    const auto response = _post(
        client, stop, revokeEndpoint,
        std::format("{}&{}&{}", field("token", refreshToken), field("token_type_hint", "refresh_token"), field("client_id", clientId))
    );
    if (!response.ok()) {
        logs::warning(
            "Session", "The platform was not told of the logout : {}", response.error.empty() ? explain(response) : response.error
        );
    }
}

Response PlatformSession::_post(HttpClient& client, const std::stop_token& stop, std::string_view endpoint, std::string_view fields) {
    Request request { .method = "POST", .url = _apiUrl + std::string { endpoint } };
    request.timeout = tokensTimeout;
    request.headers = {
        { .name = "Content-Type", .value = "application/x-www-form-urlencoded" },
        { .name = "Accept-Language", .value = std::string { i18n::languageCode() } },
    };
    request.body = std::string { fields };

    auto response = client.send(request, stop);
    if (response.ok() && endpoint == tokenEndpoint) _keepTokens(response);
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
    yyjson_mut_obj_add_strncpy(stored.get(), object, "access", access.data(), access.size());
    yyjson_mut_obj_add_strncpy(stored.get(), object, "refresh", refresh.data(), refresh.size());
    yyjson_mut_obj_add_sint(stored.get(), object, "accessExpiresAt", expiresAt);

    // works until the next start : the user then logs in again
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
    if (json::string(yyjson_obj_get(root, "host")) != _hostUrl) return;

    _accessToken = json::string(yyjson_obj_get(root, "access"));
    _refreshToken = json::string(yyjson_obj_get(root, "refresh"));
    _accessExpiresAt = json::integer(yyjson_obj_get(root, "accessExpiresAt"));
}

}  // namespace net

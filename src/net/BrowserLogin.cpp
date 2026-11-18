// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BrowserLogin.h"

#include <yyjson.h>

#include <chrono>
#include <format>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

#include "HttpClient.h"
#include "LoopbackRedirect.h"
#include "Pkce.h"
#include "PlatformSession.h"
#include "src/helpers/Json.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "version.h"

namespace net {

namespace {

// time to log in, and to create an account first
constexpr std::chrono::minutes loginTimeout { 15 };

// what the browser shows once back : the rest happens in SoundBuddy
std::string backPage() {
    return std::format(
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>{0}</title></head>"
        "<body style=\"font-family: system-ui, sans-serif; text-align: center; margin-top: 20vh\"><p>{1}</p></body></html>",
        APP_NAME, i18n::format(i18n::tr("You can close this page, and go back to %1."), { APP_NAME })
    );
}

}  // namespace

BrowserLogin::BrowserLogin(std::shared_ptr<PlatformSession> session, Callbacks callbacks) :
    _session(std::move(session)),
    _callbacks(std::move(callbacks)),
    _client(APP_NAME "/" APP_CURRENT_VERSION),
    _worker([this](const std::stop_token& stop) { _run(stop); }) {}

BrowserLogin::~BrowserLogin() = default;

// runs on the worker thread
void BrowserLogin::_run(const std::stop_token& stop) {
    const auto fail = [this](const std::string& why) {
        logs::warning("Login", "{}", why);
        _callbacks.onFailed(why);
    };

    const LoopbackRedirect redirect;
    if (!redirect.isListening()) {
        fail(std::string { i18n::tr("The browser cannot come back to this computer : no port to listen on.") });
        return;
    }

    // the state ties the browser's return to this login, the verifier proves the code is ours
    const auto state = pkce::randomValue();
    const auto verifier = pkce::randomValue();
    _callbacks.onUrl(_session->authorizationUrl(redirect.redirectUri(), state, pkce::challengeOf(verifier)));

    const auto query = redirect.wait(backPage(), loginTimeout, stop);
    if (stop.stop_requested()) return;
    if (!query) {
        fail(std::string { i18n::tr("The browser did not come back in time.") });
        return;
    }

    if (LoopbackRedirect::parameter(*query, "state") != state) {
        fail(std::string { i18n::tr("The browser came back from another login.") });
        return;
    }
    const auto code = LoopbackRedirect::parameter(*query, "code");
    if (code.empty()) {
        const auto error = LoopbackRedirect::parameter(*query, "error");
        fail(error == "access_denied" ? std::string { i18n::tr("Access was denied in the browser.") } : error);
        return;
    }

    const auto tokens = _session->login(_client, code, verifier, redirect.redirectUri(), stop);
    if (stop.stop_requested()) return;
    if (!tokens.ok()) {
        fail(tokens.error.empty() ? PlatformSession::explain(tokens) : tokens.error);
        return;
    }

    // who logged in : the tokens do not tell SoundBuddy
    Request me { .method = "GET", .url = _session->apiUrl() + "/api/v1/me" };
    me.headers = { { .name = "Accept-Language", .value = std::string { i18n::languageCode() } } };
    const auto response = _session->send(_client, me, stop);
    if (stop.stop_requested()) return;

    const auto doc = json::parse(response.body);
    const std::string username { json::string(yyjson_obj_get(json::root(doc), "username")) };
    if (!response.ok() || username.empty()) {
        fail(response.error.empty() ? PlatformSession::explain(response) : response.error);
        return;
    }

    logs::info("Login", "Logged in as \"{}\"", username);
    _callbacks.onLoggedIn(username);
}

}  // namespace net

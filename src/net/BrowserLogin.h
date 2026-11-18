// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "HttpClient.h"
#include "PlatformSession.h"

namespace net {

// Logging in through the browser (OAuth's authorization code with PKCE, RFC 7636 and 8252), on a thread of its own : the user
// logs in on the platform's page, which sends the browser back to SoundBuddy (src/net/LoopbackRedirect.h) with a code, that
// "session" exchanges for tokens. Stopped by its destruction, as when the user cancels
class BrowserLogin {
 public:
    // called from the login's thread
    struct Callbacks {
        // where the browser must go : to open, and to show in case the browser did not
        std::function<void(const std::string& url)> onUrl;
        // logged in, as "username"
        std::function<void(const std::string& username)> onLoggedIn;
        // why it did not work (translated)
        std::function<void(const std::string& why)> onFailed;
    };

    BrowserLogin(std::shared_ptr<PlatformSession> session, Callbacks callbacks);
    ~BrowserLogin();

    BrowserLogin(const BrowserLogin&) = delete;
    void operator=(const BrowserLogin&) = delete;

 private:
    const std::shared_ptr<PlatformSession> _session;
    const Callbacks _callbacks;
    HttpClient _client;
    std::jthread _worker;  // last : stopped and joined first

    void _run(const std::stop_token& stop);
};

}  // namespace net

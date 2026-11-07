// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>

#include "HttpClient.h"
#include "src/helpers/SingleFlight.h"

namespace net {

// The login to the platform (OAuth tokens, RFC 6749), shared by whatever calls it : the password is exchanged once, then forgotten.
// A refresh token works only once, and using one twice ends the login : so one thread at a time gets tokens, the others waiting for them
class PlatformSession {
 public:
    // tokens of "tokensFile" are for this host and user, or ignored. A "password" replaces them : "onPasswordUsed" tells once it was
    // accepted or refused, as there is no use in keeping it. It stays for what can be tried again (platform unreachable, too many tries)
    PlatformSession(
        const std::string& hostUrl, std::string username, std::filesystem::path tokensFile, std::string password = {},
        std::function<void()> onPasswordUsed = {}
    );

    // whether there is anything to log in with
    bool hasLogin() const;

    // "request", with the access token, renewed first when needed. Without a valid login, the response is a 403 telling why;
    // when tokens could not be had for now (platform unreachable, too many tries), what the platform answered about them
    Response send(HttpClient& client, const Request& request, const std::stop_token& stop = {});

    // where the API is, for this host : in HTTPS, or in HTTP for a platform running locally
    const std::string& apiUrl() const { return _apiUrl; }

    static std::string apiUrlOf(std::string_view hostUrl);

    // what the platform says of a failure, as OAuth (RFC 6749) and the API (RFC 9457) write it, else the body as it is
    static std::string explain(const Response& response);

 private:
    const std::string _hostUrl;
    const std::string _apiUrl;
    const std::string _username;
    const std::filesystem::path _tokensFile;
    const std::function<void()> _onPasswordUsed;

    // guards what follows. Never held during a request : the user interface reads hasLogin(), and must not wait for the platform
    mutable std::mutex _mutex;
    SingleFlight _exchange;  // one request for tokens at a time, as a refresh token works once
    std::string _password;
    std::string _accessToken;
    std::string _refreshToken;
    std::int64_t _accessExpiresAt = 0;  // seconds since the epoch

    struct Access {
        std::string token;  // empty when there is none
        Response failure;
    };

    Access _access(HttpClient& client, const std::stop_token& stop, const std::string& tokenRefused);

    // the request for tokens, on its turn, with the lock free
    Access _exchangeTokens(HttpClient& client, const std::stop_token& stop, const std::string& password, const std::string& refreshToken);

    // "fields" sent to the token endpoint : a 200 keeps the tokens it comes with
    Response _askTokens(HttpClient& client, const std::stop_token& stop, std::string_view fields);
    void _keepTokens(const Response& response);
    void _forgetTokens();
    void _loadTokens();
};

}  // namespace net

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>

#include "HttpClient.h"
#include "src/helpers/SingleFlight.h"

namespace net {

// The login to the platform (OAuth tokens, RFC 6749), shared by whatever calls it : the user logs in through the browser
// (src/net/BrowserLogin.h), whose code is exchanged here for tokens. A refresh token works only once, and using one twice ends the
// login : so one thread at a time gets tokens, the others waiting for them
class PlatformSession {
 public:
    // tokens of "tokensFile" are for this host, or ignored
    PlatformSession(const std::string& hostUrl, std::filesystem::path tokensFile);

    // whether there is anything to log in with
    bool hasLogin() const;

    // "request", with the access token, renewed first when needed. Without a valid login, the response is a 403 telling why;
    // when tokens could not be had for now (platform unreachable, too many tries), what the platform answered about them
    Response send(HttpClient& client, const Request& request, const std::stop_token& stop = {});

    // where the browser goes to log in, asking for the API's tokens : "challenge" being the PKCE one of the verifier given to login()
    std::string authorizationUrl(std::string_view redirectUri, std::string_view state, std::string_view challenge) const;

    // the code the browser came back with, exchanged for tokens, which replace those there were. A failure is a 403 telling why
    // when the platform refused it, else what it answered
    Response login(
        HttpClient& client, std::string_view code, std::string_view verifier, std::string_view redirectUri, const std::stop_token& stop = {}
    );

    // forgets the login, and tells the platform it is over : its refresh token stops working there too
    void logout(HttpClient& client, const std::stop_token& stop = {});

    // where the API is, for this host : in HTTPS, or in HTTP for a platform running locally
    const std::string& apiUrl() const { return _apiUrl; }

    // what the tokens are for (RFC 8707) : the platform signs them for the API only
    std::string resource() const { return _apiUrl + "/api/v1"; }

    static std::string apiUrlOf(std::string_view hostUrl);

    // what the platform says of a failure, as OAuth (RFC 6749) and the API (RFC 9457) write it, else the body as it is
    static std::string explain(const Response& response);

 private:
    const std::string _hostUrl;
    const std::string _apiUrl;
    const std::filesystem::path _tokensFile;

    // guards what follows. Never held during a request : the user interface reads hasLogin(), and must not wait for the platform
    mutable std::mutex _mutex;
    SingleFlight _exchange;  // one request for tokens at a time, as a refresh token works once
    std::string _accessToken;
    std::string _refreshToken;
    std::int64_t _accessExpiresAt = 0;  // seconds since the epoch

    struct Access {
        std::string token;  // empty when there is none
        Response failure;
    };

    Access _access(HttpClient& client, const std::stop_token& stop, const std::string& tokenRefused);

    // the refresh, on its turn, with the lock free
    Access _refresh(HttpClient& client, const std::stop_token& stop, const std::string& refreshToken);

    // "fields" sent to "endpoint" of OAuth : a 200 of the token endpoint keeps the tokens it comes with
    Response _post(HttpClient& client, const std::stop_token& stop, std::string_view endpoint, std::string_view fields);
    void _keepTokens(const Response& response);
    void _forgetTokens();
    void _loadTokens();
};

}  // namespace net

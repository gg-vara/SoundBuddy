// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// What logging in through the browser relies on : src/net/Pkce.h, src/net/LoopbackRedirect.h

#include <chrono>
#include <future>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "Check.h"
#include "src/net/HttpClient.h"
#include "src/net/LoopbackRedirect.h"
#include "src/net/Pkce.h"

int main() {
    using net::LoopbackRedirect;

    // RFC 7636, appendix B
    CHECK(
        net::pkce::challengeOf("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk") == "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM",
        "S256 challenge"
    );

    const auto first = net::pkce::randomValue();
    CHECK(
        first.size() == 43 &&
            first.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_") == std::string::npos,
        "a verifier : 43 characters of base64url"
    );
    CHECK(first != net::pkce::randomValue(), "never twice the same");

    CHECK(LoopbackRedirect::parameter("code=a%2Bb+c&state=s", "code") == "a+b c", "a parameter, decoded");
    CHECK(LoopbackRedirect::parameter("code=x&state=s%26t", "state") == "s&t", "the last one");
    CHECK(LoopbackRedirect::parameter("error=access_denied", "code").empty(), "missing : empty");

    {
        LoopbackRedirect redirect;
        CHECK(redirect.isListening(), "listens");
        CHECK(
            redirect.redirectUri().starts_with("http://127.0.0.1:") && redirect.redirectUri().ends_with("/callback"),
            "on this computer only"
        );

        // as a browser : first what is not the callback, then the callback
        auto browser = std::async(std::launch::async, [uri = redirect.redirectUri()] {
            net::HttpClient client { "test" };
            const auto base = uri.substr(0, uri.find("/callback"));
            const auto favicon = client.send({ .url = base + "/favicon.ico" });
            const auto callback = client.send({ .url = uri + "?code=the%20code&state=s" });
            return std::make_pair(favicon, callback);
        });

        const auto query = redirect.wait("<p>Back to the app</p>", std::chrono::seconds { 10 }, {});
        const auto [favicon, callback] = browser.get();
        CHECK(query == std::optional<std::string> { "code=the%20code&state=s" }, "the callback's query");
        CHECK(favicon.status == 404, "anything else : 404");
        CHECK(callback.status == 200 && callback.body == "<p>Back to the app</p>", "the browser gets the page");
    }

    {
        LoopbackRedirect redirect;
        std::stop_source stop;
        const std::jthread stopper { [&stop] {
            std::this_thread::sleep_for(std::chrono::milliseconds { 300 });
            stop.request_stop();
        } };
        const auto started = std::chrono::steady_clock::now();
        const auto query = redirect.wait("", std::chrono::seconds { 30 }, stop.get_token());
        CHECK(!query && std::chrono::steady_clock::now() - started < std::chrono::seconds { 5 }, "stops when asked");
    }

    {
        LoopbackRedirect redirect;
        CHECK(!redirect.wait("", std::chrono::seconds { 1 }, {}), "gives up after its time");
    }

    return check::summary();
}

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The login to the platform : where it goes, what it tells, and what is stored. Its exchanges are in headless.test.ts

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <stop_token>
#include <string>

#include "Check.h"
#include "src/helpers/SecretStore.h"
#include "src/net/HttpClient.h"
#include "src/net/PlatformSession.h"

namespace {

const auto file = std::filesystem::temp_directory_path() / "soundbuddy-session-test.dat";

}  // namespace

int main() {
    using net::PlatformSession;

    // where the API is : a password or token never goes in clear, but to this computer
    CHECK(PlatformSession::apiUrlOf("https://soundvitrine.com") == "https://soundvitrine.com", "HTTPS kept");
    CHECK(PlatformSession::apiUrlOf("http://soundvitrine.com") == "https://soundvitrine.com", "HTTP upgraded to HTTPS");
    CHECK(PlatformSession::apiUrlOf("soundvitrine.com") == "https://soundvitrine.com", "HTTPS added");
    CHECK(PlatformSession::apiUrlOf("https://soundvitrine.com/") == "https://soundvitrine.com", "no trailing slash");
    CHECK(
        PlatformSession::apiUrlOf("https://localhost:3000") == "http://localhost:3000" &&
            PlatformSession::apiUrlOf("127.0.0.1") == "http://127.0.0.1",
        "HTTP for a local platform"
    );

    // what the platform says
    net::Response response;
    response.body = R"({"error":"invalid_grant","error_description":"Wrong password."})";
    CHECK(PlatformSession::explain(response) == "Wrong password.", "explains as OAuth does");
    response.body = R"({"type":"about:blank","title":"FORBIDDEN","status":403,"detail":"Not allowed"})";
    CHECK(PlatformSession::explain(response) == "Not allowed", "explains as the API does");
    response.body = "plain text";
    CHECK(PlatformSession::explain(response) == "plain text", "the body otherwise");

    // a secret, kept unreadable
    const std::string secret = "a very secret token \xC3\xA9";
    secrets::remove(file);
    CHECK(!secrets::load(file).has_value(), "nothing to begin with");
    CHECK(secrets::store(file, secret), "stored");
    CHECK(secrets::load(file) == secret, "read back");
    CHECK(secrets::store(file, "another"), "replaced");
    CHECK(secrets::load(file) == "another", "read back replaced");

#ifdef _WIN32
    secrets::store(file, secret);
    std::ifstream stream { file, std::ios::binary };
    const std::string raw { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    stream.close();
    CHECK(!raw.empty() && raw.find("secret") == std::string::npos, "not in clear on disk");
#endif

    secrets::remove(file);
    CHECK(!secrets::load(file).has_value(), "removed");

    // tokens of another host are not sent to this one
    secrets::remove(file);
    CHECK(!PlatformSession("https://host.example", file).hasLogin(), "no tokens, no login");
    secrets::store(file, R"({"host":"https://host.example","access":"a","refresh":"r","accessExpiresAt":0})");
    CHECK(PlatformSession("https://host.example", file).hasLogin(), "tokens are a login");
    CHECK(!PlatformSession("https://other.example", file).hasLogin(), "not for another host");

    // where the browser goes : the API's tokens, with PKCE, for SoundBuddy, coming back to this computer
    const auto url = PlatformSession("soundvitrine.example", file).authorizationUrl("http://127.0.0.1:1234/callback", "st", "ch");
    CHECK(url.starts_with("https://soundvitrine.example/api/auth/oauth2/authorize?response_type=code&client_id=soundbuddy&"), "authorize");
    CHECK(url.find("&redirect_uri=http%3A%2F%2F127.0.0.1%3A1234%2Fcallback&") != std::string::npos, "the redirect, encoded");
    CHECK(url.find("&code_challenge=ch&code_challenge_method=S256&") != std::string::npos, "PKCE");
    CHECK(url.find("&scope=library%3Awrite%20shout%3Awrite%20offline_access&") != std::string::npos, "what is asked");
    CHECK(url.ends_with("&resource=https%3A%2F%2Fsoundvitrine.example%2Fapi%2Fv1"), "for the API");

    // the logout forgets, even when the platform cannot be told : here, a port where nothing listens
    secrets::store(file, R"({"host":"http://127.0.0.1:9","access":"a","refresh":"r","accessExpiresAt":0})");
    {
        PlatformSession session { "http://127.0.0.1:9", file };
        CHECK(session.hasLogin(), "logged in");
        net::HttpClient client { "test" };
        session.logout(client, std::stop_token {});
        CHECK(!session.hasLogin() && !secrets::load(file).has_value(), "logged out");
    }
    secrets::remove(file);

    return check::summary();
}

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The login to the platform : where it goes, what it tells, and what is stored. Its exchanges are in headless.test.ts

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
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

    // a login : a password counts, as tokens would, and replaces those there were
    secrets::store(file, "{}");
    {
        const PlatformSession session { "https://host.example", "user", file, "password" };
        CHECK(session.hasLogin(), "a password is a login");
        CHECK(!secrets::load(file).has_value(), "it replaces the previous tokens");
    }

    // tokens of another host or user are not sent to this one
    const std::string tokens = R"({"host":"https://host.example","username":"user","access":"a","refresh":"r","accessExpiresAt":0})";
    secrets::store(file, tokens);
    CHECK(PlatformSession("https://host.example", "user", file).hasLogin(), "tokens are a login");
    CHECK(!PlatformSession("https://other.example", "user", file).hasLogin(), "not for another host");
    CHECK(!PlatformSession("https://host.example", "other", file).hasLogin(), "not for another user");
    secrets::remove(file);

    return check::summary();
}

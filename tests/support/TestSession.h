// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "src/helpers/SecretStore.h"
#include "src/settings/AppSettings.h"
#include "src/net/PlatformSession.h"

// what a worker gets to reach "host" : a login whose access token has expired, to renew there first
inline AppSettings::ConnectivityInfos connectedTo(const std::string& host) {
    const auto tokensFile = std::filesystem::temp_directory_path() / "soundbuddy-test-session.dat";
    secrets::store(tokensFile, R"({"host":")" + host + R"(","access":"a","refresh":"r","accessExpiresAt":0})");
    return {
        .platformHostUrl = host, .username = "user", .session = std::make_shared<net::PlatformSession>(host, tokensFile), .areOK = true
    };
}

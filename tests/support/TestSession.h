// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "src/settings/AppSettings.h"
#include "src/net/PlatformSession.h"

// what a worker gets to reach "host", with a password yet to exchange
inline AppSettings::ConnectivityInfos connectedTo(const std::string& host) {
    return { .platformHostUrl = host,
             .username = "user",
             .session = std::make_shared<net::PlatformSession>(
                 host, "user", std::filesystem::temp_directory_path() / "soundbuddy-test-session.dat", "password"
             ),
             .areOK = true };
}

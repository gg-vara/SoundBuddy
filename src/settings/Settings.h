// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Application settings, stored as a JSON file
namespace settings {

struct Settings {
    // SoundVitrine connectivity : who logged in through the browser, the tokens being kept apart (src/net/PlatformSession.h)
    std::string platformHostUrl;
    std::string platformUsername;

    // automation
    bool autoRunShout = false;
    bool runAtStartup = false;
};

// settings stored in "file", or nothing if it does not exist or is not a JSON object; missing values keep their defaults
std::optional<Settings> load(const std::filesystem::path& file);

// writes "file", creating its directory if needed; a failure leaves the previous file untouched
bool save(const std::filesystem::path& file, const Settings& settings);

}  // namespace settings

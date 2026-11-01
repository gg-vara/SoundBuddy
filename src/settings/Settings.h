// SoundBuddy
// Small companion app that feeds SoundVitrine with ITunes / Music library metadata
// Copyright (C) 2019-2023 Guillaume Vara <guillaume.vara@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Any graphical or audio resources available within the source code may
// use a different license and copyright : please refer to their metadata
// for further details. Resources without explicit references to a
// different license and copyright still refer to this GPL.

#pragma once

#include <filesystem>
#include <optional>
#include <string>

// Application settings, stored as a JSON file
namespace settings {

struct Settings {
    // SoundVitrine connectivity
    std::string platformHostUrl;
    std::string platformUsername;
    std::string platformPassword;

    // automation
    bool autoRunShout = false;
    bool runAtStartup = false;
};

// settings stored in "file", or nothing if it does not exist or is not a JSON object; missing values keep their defaults
std::optional<Settings> load(const std::filesystem::path& file);

// writes "file", creating its directory if needed; a failure leaves the previous file untouched
bool save(const std::filesystem::path& file, const Settings& settings);

}  // namespace settings

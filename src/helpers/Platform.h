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
#include <string>
#include <string_view>

// Operating system services, through native APIs
namespace platform {

// directory where the app stores its data (settings, library exports), created if missing
std::filesystem::path dataDirectory();

// opens "path" with its default app (a directory in the file explorer); false on failure
bool openPath(const std::filesystem::path& path);

// opens "url" (UTF-8) in the default browser; false on failure
bool openUrl(const std::string& url);

// whether the session starts this executable at login
bool isLaunchingAtStartup();

// makes the session start this executable at login, or not; false on failure
bool setLaunchingAtStartup(bool enabled);

// whether this process is the only instance of the app running in the user session.
// The first caller keeps the claim until it exits (or crashes)
bool claimSingleInstance();

// XML library file shared by iTunes / Music, deduced from its preferences.
// Throws std::runtime_error, with a translated message, if it cannot be deduced
std::filesystem::path musicLibraryFile();

// UTF-8 conversions of paths, whatever the platform's native encoding
std::string toUtf8(const std::filesystem::path& path);
std::filesystem::path fromUtf8(std::string_view text);

}  // namespace platform

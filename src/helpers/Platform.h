// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

// name of the music app SoundBuddy works with : iTunes on Windows, Music on macOS (translated, UTF-8)
std::string_view musicAppName();

// XML library file shared by iTunes / Music, deduced from its preferences.
// Throws std::runtime_error, with a translated message, if it cannot be deduced
std::filesystem::path musicLibraryFile();

// UTF-8 conversions of paths, whatever the platform's native encoding
std::string toUtf8(const std::filesystem::path& path);
std::filesystem::path fromUtf8(std::string_view text);

#ifdef _WIN32
// conversions between UTF-8 and the UTF-16 of Windows APIs
std::string narrow(std::wstring_view text);
std::wstring widen(std::string_view text);
#endif

}  // namespace platform

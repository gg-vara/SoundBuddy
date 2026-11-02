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

#ifdef _WIN32

#include "Platform.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>

#include "src/i18n/Translations.h"
#include "version.h"

namespace platform {

namespace {

// programs started at login, for the current user
constexpr const wchar_t* runKey = LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
constexpr const wchar_t* runValue = L"" APP_NAME;

std::filesystem::path knownFolder(REFKNOWNFOLDERID id) {
    PWSTR path = nullptr;
    std::filesystem::path folder;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &path))) folder = path;
    CoTaskMemFree(path);
    return folder;
}

std::filesystem::path applicationFile() {
    std::wstring path(MAX_PATH, L'\0');
    while (true) {
        const auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size()) {
            path.resize(length);
            return path;
        }
        path.resize(path.size() * 2);  // truncated
    }
}

bool shellOpen(const std::wstring& target) {
    auto* const result = ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

// command registered to start the app at login, or nothing
std::wstring registeredStartupCommand() {
    DWORD size = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, runKey, runValue, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS) return {};

    std::wstring command(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, runKey, runValue, RRF_RT_REG_SZ, nullptr, command.data(), &size) != ERROR_SUCCESS) return {};

    command.resize(wcsnlen(command.c_str(), command.size()));
    return command;
}

// startup command of this executable : quoted, as Windows does
std::wstring startupCommand() {
    return L'"' + applicationFile().wstring() + L'"';
}

// preferences file of iTunes, Microsoft Store version first
std::filesystem::path iTunesPreferencesFile() {
    const std::filesystem::path relative = LR"(Apple Computer\Preferences\com.apple.iTunes.plist)";

    std::error_code error;
    std::filesystem::directory_iterator packages { knownFolder(FOLDERID_LocalAppData) / "Packages", error };
    for (; !error && packages != std::filesystem::directory_iterator(); packages.increment(error)) {
        if (packages->path().filename().wstring().starts_with(L"AppleInc.iTunes") && packages->is_directory(error)) {
            return packages->path() / "LocalCache" / "Roaming" / relative;
        }
    }

    return knownFolder(FOLDERID_RoamingAppData) / relative;
}

// "text" with its %XX escapes decoded
std::string percentDecoded(std::string_view text) {
    const auto hexValue = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    std::string decoded;
    decoded.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const auto high = hexValue(text[i + 1]);
            const auto low = hexValue(text[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded += static_cast<char>((high * 16) + low);
                i += 2;
                continue;
            }
        }
        decoded += text[i];
    }

    return decoded;
}

}  // namespace

std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    const auto size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    const auto size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::filesystem::path dataDirectory() {
    const auto directory = knownFolder(FOLDERID_LocalAppData) / APP_PUBLISHER_ORG / APP_NAME;

    std::error_code error;
    std::filesystem::create_directories(directory, error);

    return directory;
}

bool openPath(const std::filesystem::path& path) {
    return shellOpen(path.wstring());
}

bool openUrl(const std::string& url) {
    return shellOpen(widen(url));
}

bool isLaunchingAtStartup() {
    const auto registered = registeredStartupCommand();
    const auto expected = startupCommand();

    // paths are case-insensitive
    return CompareStringOrdinal(
               registered.c_str(), static_cast<int>(registered.size()), expected.c_str(), static_cast<int>(expected.size()), TRUE
           ) == CSTR_EQUAL;
}

bool setLaunchingAtStartup(bool enabled) {
    if (!enabled) {
        const auto result = RegDeleteKeyValueW(HKEY_CURRENT_USER, runKey, runValue);
        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

    const auto command = startupCommand();
    return RegSetKeyValueW(
               HKEY_CURRENT_USER, runKey, runValue, REG_SZ, command.c_str(), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))
           ) == ERROR_SUCCESS;
}

std::string_view musicAppName() {
    return "iTunes";
}

bool claimSingleInstance() {
    // the mutex is released by the system when the process ends; its handle is never closed before
    static const bool claimed = [] {
        auto* const mutex = CreateMutexW(nullptr, FALSE, L"Local\\" APP_PUBLISHER_ORG "." APP_NAME);
        return mutex != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
    }();
    return claimed;
}

std::filesystem::path musicLibraryFile() {
    std::ifstream preferences { iTunesPreferencesFile(), std::ios::binary };
    if (!preferences) {
        throw std::runtime_error(i18n::format(i18n::tr("Could not open %1 preferences file."), { musicAppName() }));
    }

    const std::string content { std::istreambuf_iterator<char>(preferences), std::istreambuf_iterator<char>() };

    // the library database is referenced as a file URL, ending with a tab
    constexpr std::string_view hint = "file://localhost/";
    constexpr std::string_view database = "iTunes%20Library.itl";

    for (auto start = content.find(hint); start != std::string::npos; start = content.find(hint, start)) {
        start += hint.size();

        const auto end = content.find('\t', start);
        if (end == std::string::npos) break;  // malformed URL

        const auto url = std::string_view(content).substr(start, end - start);
        const auto databaseAt = url.find(database);
        if (databaseAt != std::string_view::npos) {
            // the XML library is shared next to the database
            return fromUtf8(percentDecoded(url.substr(0, databaseAt))) / "iTunes Music Library.xml";
        }
    }

    throw std::runtime_error(i18n::format(i18n::tr("%1 XML library location could not be deduced."), { musicAppName() }));
}

}  // namespace platform

#endif

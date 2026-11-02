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

#include "AppSettings.h"

#include <windows.h>

#include <string>

namespace {

// key where QSettings stored the values : HKEY_CURRENT_USER\Software\<organization>\<application>
const wchar_t* const legacyKey = L"Software\\" APP_PUBLISHER_ORG "\\" APP_NAME;

// string value written by QSettings, UTF-8, or nothing
std::optional<std::string> readString(HKEY key, const wchar_t* name) {
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ) return std::nullopt;

    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, nullptr, reinterpret_cast<BYTE*>(value.data()), &size) != ERROR_SUCCESS) return std::nullopt;

    value.resize(value.find(L'\0') == std::wstring::npos ? size / sizeof(wchar_t) : value.find(L'\0'));

    // QSettings escapes texts starting with '@' (its marker of serialized values) by doubling it
    if (value.starts_with(L"@@")) value.erase(0, 1);
    return platform::narrow(value);
}

// QSettings stores booleans as "true" / "false" texts
bool readBoolean(HKEY key, const wchar_t* name) {
    return readString(key, name) == "true";
}

}  // namespace

std::optional<settings::Settings> AppSettings::_readLegacySettings() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, legacyKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return std::nullopt;

    DWORD valueCount = 0;
    RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &valueCount, nullptr, nullptr, nullptr, nullptr);

    std::optional<settings::Settings> legacy;
    if (valueCount > 0) {
        legacy = settings::Settings {
            .platformHostUrl = readString(key, L"PlatformHostUrl").value_or(""),
            .platformUsername = readString(key, L"PlatformUsername").value_or(""),
            .platformPassword = readString(key, L"PlatformPassword").value_or(""),
            .autoRunShout = readBoolean(key, L"AutoRunShout"),
            .runAtStartup = readBoolean(key, L"MustRunAtStartup"),
        };
    }

    RegCloseKey(key);
    return legacy;
}

#endif

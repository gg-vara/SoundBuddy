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

#include "AppSettings.h"

#include <QSettings>

AppSettings::AppSettings() : _file(platform::dataDirectory() / "settings.json") {
    if (auto stored = settings::load(_file)) {
        _values = std::move(*stored);
    } else if (auto legacy = _readLegacySettings()) {
        _values = std::move(*legacy);
        save();
    }
}

// To remove with Qt
std::optional<settings::Settings> AppSettings::_readLegacySettings() {
    const QSettings legacy;  // organization and application names are set in main()
    if (legacy.allKeys().isEmpty()) return std::nullopt;

    return settings::Settings {
        .platformHostUrl = legacy.value("PlatformHostUrl").toString().toStdString(),
        .platformUsername = legacy.value("PlatformUsername").toString().toStdString(),
        .platformPassword = legacy.value("PlatformPassword").toString().toStdString(),
        .autoRunShout = legacy.value("AutoRunShout").toBool(),
        .runAtStartup = legacy.value("MustRunAtStartup").toBool(),
    };
}

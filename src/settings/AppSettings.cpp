// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppSettings.h"

#include <functional>
#include <memory>
#include <utility>

#include "src/helpers/Platform.h"
#include "src/net/PlatformSession.h"
#include "src/settings/Settings.h"

AppSettings::AppSettings() :
    _file(platform::dataDirectory() / "settings.json") {
    if (auto stored = settings::load(_file)) {
        _values = std::move(*stored);
    } else if (!platform::isIsolated()) {
        if (auto legacy = _readLegacySettings()) {
            _values = std::move(*legacy);
            save();
        }
    }
}

void AppSettings::startSession(std::function<void()> onPasswordUsed) {
    _session = std::make_shared<net::PlatformSession>(
        _values.platformHostUrl, _values.platformUsername, platform::dataDirectory() / "session.dat", _values.platformPassword,
        std::move(onPasswordUsed)
    );
}

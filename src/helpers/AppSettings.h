// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "src/helpers/Platform.h"
#include "src/net/PlatformSession.h"
#include "src/settings/Settings.h"
#include "version.h"

class AppSettings {
 public:
    // loads the settings file, or imports the settings of the previous versions on first launch
    AppSettings();

    settings::Settings& values() { return _values; }

    const settings::Settings& values() const { return _values; }

    // writes the current values to the settings file
    bool save() const { return settings::save(_file, _values); }

    // texts are UTF-8
    struct ConnectivityInfos {
        std::string platformHostUrl;
        std::string username;

        // how to call the platform, as long as the login lasts
        std::shared_ptr<net::PlatformSession> session;

        // Whenever infos are considered valid for usage
        bool areOK = false;

        std::string getPlaformHomeUrl() const {
            if (!areOK) return {};
            return platformHostUrl + "/u/" + username;
        }
    };

    ConnectivityInfos getConnectivityInfos() const {
        if (_values.platformUsername.empty() || _values.platformHostUrl.empty() || !_session || !_session->hasLogin()) return {};

        return { .platformHostUrl = _values.platformHostUrl, .username = _values.platformUsername, .session = _session, .areOK = true };
    }

    // from the current values, the password being exchanged for tokens : "onPasswordUsed" tells when it can go.
    // Workers keep the session they got for as long as they run
    void startSession(std::function<void()> onPasswordUsed);

#ifdef __APPLE__
    // preferences domain where QSettings stored the settings of previous versions
    static std::string legacyDomain();
#endif

    static std::filesystem::path getFeedWarningFilePath() { return platform::dataDirectory() / feedWarningFileName; }

    // format of the files uploaded to the platform
    enum class FeedFormat {
        Json,
        Compressed  // compressed JSON library (see compression::compress())
    };

    static std::filesystem::path getFeedOutputFilePath(FeedFormat format) {
        return platform::dataDirectory() / (std::string(feedOutputBaseFileName) + std::string(getFeedOutputExtension(format)));
    }

    static std::string_view getFeedOutputExtension(FeedFormat format) {
        return format == FeedFormat::Compressed ? zippedFeedOutputExtension : basicFeedOutputExtension;
    }

    static std::string_view getFeedOutputContentType(FeedFormat format) {
        return format == FeedFormat::Compressed ? "application/octet-stream" : "application/json";
    }

 private:
    std::filesystem::path _file;
    settings::Settings _values;
    std::shared_ptr<net::PlatformSession> _session;

    // previous versions stored their settings with QSettings (registry on Windows, plist on macOS) : read once, to create the settings file
    static std::optional<settings::Settings> _readLegacySettings();

    static inline const char* feedWarningFileName = "warnings.json";

    static inline const char* feedOutputBaseFileName = "output";
    static inline const char* zippedFeedOutputExtension = ".zmlib";
    static inline const char* basicFeedOutputExtension = ".json";
};

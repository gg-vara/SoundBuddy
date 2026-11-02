#pragma once

#include <QSettings>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "src/helpers/Platform.h"
#include "src/settings/Settings.h"
#include "version.h"

class AppSettings {
 public:
    // loads the settings file, or imports the settings of the previous versions on first launch
    AppSettings() : _file(platform::dataDirectory() / "settings.json") {
        if (auto stored = settings::load(_file)) {
            _values = std::move(*stored);
        } else if (auto legacy = _readLegacySettings()) {
            _values = std::move(*legacy);
            save();
        }
    }

    settings::Settings& values() {
        return _values;
    }

    const settings::Settings& values() const {
        return _values;
    }

    // writes the current values to the settings file
    bool save() const {
        return settings::save(_file, _values);
    }

    // texts are UTF-8
    struct ConnectivityInfos {
        const std::string platformHostUrl;
        const std::string username;
        const std::string password;

        // Whenever infos are considered valid for usage
        bool areOK = false;


        std::string getSoundHeartbeatUrl() const {
            if(!areOK) return {};
            return platformHostUrl + "/hb";
        }

        std::string getPlaformHomeUrl() const {
            if(!areOK) return {};
            return platformHostUrl + "/u/" + username;
        }
    };

    struct UploadInfos {
        const std::string path;
        const std::string outputFileName;
    };

    ConnectivityInfos getConnectivityInfos() const {
        if(_values.platformUsername.empty() || _values.platformHostUrl.empty() || _values.platformPassword.empty())
            return {};

        return {
            _values.platformHostUrl,
            _values.platformUsername,
            _values.platformPassword,
            true
        };
    }

    static UploadInfos getShoutUploadInfos() {
        return { "/uploadShout", "shout_file" };
    }

    static UploadInfos getFeederUploadInfos() {
        return { "/uploadMusicLibrary", DEST_PLATFORM_PRODUCT_NAME "_file" };
    }

    static std::filesystem::path getFeedWarningFilePath() {
        return platform::dataDirectory() / _FeedWarningFileName;
    }

    static std::filesystem::path getFeedOutputFilePath(bool isCompressed) {
        return platform::dataDirectory() / (std::string(_FeedOutputBaseFileName) + (isCompressed ? _ZippedFeedOutputExtension : _BasicFeedOutputExtension));
    }

    static std::string_view getFeedOutputExtension(bool isCompressed) {
        return isCompressed ? _ZippedFeedOutputExtension : _BasicFeedOutputExtension;
    }

    static std::string_view getFeedOutputContentType(bool isCompressed) {
        return isCompressed ? "application/compressed-mlib" : "application/json";
    }

 private:
    std::filesystem::path _file;
    settings::Settings _values;

    // previous versions stored their settings with QSettings (registry on Windows, plist on macOS) : read once, to create the settings file.
    // To remove with Qt.
    static std::optional<settings::Settings> _readLegacySettings() {
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

    static inline const char * _FeedWarningFileName = "warnings.json";

    static inline const char * _FeedOutputBaseFileName = "output";
    static inline const char * _ZippedFeedOutputExtension = ".zmlib";
    static inline const char * _BasicFeedOutputExtension = ".json";
};

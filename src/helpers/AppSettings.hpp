#pragma once

#include <QSettings>
#include <QVariant>

#include <filesystem>
#include <optional>
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

    struct ConnectivityInfos {
        const QString platformHostUrl = QString();
        const QString username = QString();
        const QString password = QString();

        // Whenever infos are considered valid for usage
        bool areOK = false;


        const QString getSoundHeartbeatUrl() const {
            if(!areOK) return QString();
            return platformHostUrl + "/hb";
        }

        const QString getPlaformHomeUrl() const {
            if(!areOK) return QString();
            return platformHostUrl + "/u/" + username;
        }
    };

    struct UploadInfos {
        const QString path;
        const QString outputFileName;
    };

    const ConnectivityInfos getConnectivityInfos() const {
        auto hostUrl = QString::fromStdString(_values.platformHostUrl);
        auto username = QString::fromStdString(_values.platformUsername);
        auto password = QString::fromStdString(_values.platformPassword);

        if(username.isEmpty() || hostUrl.isEmpty() || password.isEmpty())
            return {};

        return {
            hostUrl,
            username,
            password,
            true
        };
    }

    static const UploadInfos getShoutUploadInfos() {
        return { "/uploadShout", "shout_file" };
    }

    static const UploadInfos getFeederUploadInfos() {
        return { "/uploadMusicLibrary", QString(DEST_PLATFORM_PRODUCT_NAME) + "_file" };
    }

    static std::filesystem::path getFeedWarningFilePath() {
        return platform::dataDirectory() / _FeedWarningFileName;
    }

    static std::filesystem::path getFeedOutputFilePath(bool isCompressed) {
        return platform::dataDirectory() / (std::string(_FeedOutputBaseFileName) + (isCompressed ? _ZippedFeedOutputExtension : _BasicFeedOutputExtension));
    }

    static const QString getFeedOutputExtension(bool isCompressed) {
        return isCompressed ? QString(_ZippedFeedOutputExtension) : QString(_BasicFeedOutputExtension);
    }

    static const QVariant getFeedOutputContentType(bool isCompressed) {
        return QVariant(isCompressed ? "application/compressed-mlib" : "application/json");
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

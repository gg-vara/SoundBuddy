#pragma once

#include <QSettings>
#include <QDir>

#include "src/helpers/PlatformHelper.h"
#include "version.h"

class AppSettings : public QSettings {
 public:
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
        auto hostUrl =  this->value(PLATFORM_HOST_URL).toString();
        auto username = this->value(PLATFORM_USERNAME).toString();
        auto password = this->value(PLATFORM_PASSWORD).toString();

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

    static const QString getFeedWarningFilePath() {
        return PlatformHelper::getDataStorageDirectory() + QDir::separator() + _FeedWarningFileName;
    }

    static const QString getFeedOutputFilePath(bool isCompressed) {
        return PlatformHelper::getDataStorageDirectory() + QDir::separator() + _FeedOutputBaseFileName + getFeedOutputExtension(isCompressed);
    }

    static const QString getFeedOutputExtension(bool isCompressed) {
        return isCompressed ? QString(_ZippedFeedOutputExtension) : QString(_BasicFeedOutputExtension);
    }

    static const QVariant getFeedOutputContentType(bool isCompressed) {
        return QVariant(isCompressed ? "application/compressed-mlib" : "application/json");
    }

    static inline const char * MUST_AUTORUN_SHOUT = "AutoRunShout";
    static inline const char * PLATFORM_USERNAME = "PlatformUsername";
    static inline const char * PLATFORM_PASSWORD = "PlatformPassword";
    static inline const char * PLATFORM_HOST_URL = "PlatformHostUrl";
    static inline const char * MUST_RUN_AT_STARTUP = "MustRunAtStartup";

 private:
    static inline const char * _FeedWarningFileName = "warnings.json";

    static inline const char * _FeedOutputBaseFileName = "output";
    static inline const char * _ZippedFeedOutputExtension = ".zmlib";
    static inline const char * _BasicFeedOutputExtension = ".json";
};

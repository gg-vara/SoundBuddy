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

#include <QThread>
#include <QString>

#include <condition_variable>
#include <mutex>

#include "src/helpers/AppSettings.hpp"
#include "src/net/HttpClient.h"
#include "src/ui/widgets/LightWidget.h"

// Periodically checks the platform credentials, until quit() or the credentials are refused
class MBeatThread : public QThread {
    Q_OBJECT

 public:
    explicit MBeatThread(const AppSettings::ConnectivityInfos &connectivityInfos);

    void run() override;

    // stops right away, even while waiting for the next heartbeat or a response
    void quit();

 signals:
    void updateConnectivityStatus(const QString &message, const ConnectivityIndicator &indic);

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    net::HttpClient _client;

    std::mutex _mutex;
    std::condition_variable _wakeUp;
    bool _stopping = false;

    net::Request _createRequest() const;
    bool _isStopping();

    // heartbeats
    static inline const std::chrono::milliseconds HEARTBEAT_INTERVAL { 30000 };
    static inline const std::chrono::milliseconds REQUEST_TIMEOUT { 3000 };
};

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

#include <QString>
#include <QJsonObject>

#include <atomic>

#include "src/workers/base/MessengerThread.hpp"
#include "src/helpers/AppSettings.hpp"
#include "src/helpers/UploadHelper.hpp"

#ifdef _WIN32
    #include <windows.h>
#endif

#include "ShoutPayload.h"
#include "ShoutPayloadConsumer.h"

class ShoutWatcher : public MessengerThread {
    Q_OBJECT

 signals:
    /** sent when a new audio file is played */
    void newAudioFileShouted(const QString &filePath, const QString &fileHash);
    void shoutReadyToSend(const QJsonObject &shoutAsJson);

 public:
    ShoutWatcher();

    void run() override;
    void quit() override;

    void processPayload(const ShoutPayload &incomingPayload);

 private:
    ShoutPayloadConsumer _consumer;
    // set from the thread calling quit()
    std::atomic<bool> _keepWatching { true };

    #ifdef _WIN32
        DWORD _oldProcessID = NULL;
    #endif

    /** returns true if a session if available, else wait. returns false to exit */
    bool _waitForAvailableSession();

    /** locks and watch for shouts */
    void _execWatchSession();
};

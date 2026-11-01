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

#include <QObject>
#include <QJsonObject>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "src/workers/base/IMessenger.hpp"
#include "src/helpers/AppSettings.hpp"
#include "src/net/HttpClient.h"

// Uploads shouts on its own thread, one at a time and in order
class ShoutUploader : public QObject, public IMessenger {
    Q_OBJECT
    Q_INTERFACES(IMessenger)

 public:
    ShoutUploader(const AppSettings::ConnectivityInfos connectivityInfos);

    // aborts the upload in progress, if any
    ~ShoutUploader();

    // queues the shout for upload
    void uploadAsShout(const QJsonObject &parsedShout);

    /** waits until either timeout or all queued shouts are uploaded */
    void waitForDrain(unsigned long timeout = 1000);

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    net::HttpClient _client;

    std::mutex _mutex;
    std::condition_variable _queueChanged;
    std::deque<std::string> _queue;
    bool _uploading = false;
    bool _stopping = false;

    // declared last : started once the members it uses are initialized
    std::thread _worker;

    void _uploadQueuedShouts();

 signals:
    void forwardMessage(
      const QString &message,
      const MessageType msgType = MessageType::STANDARD,
      const bool replacePreviousLine = false
   );
};

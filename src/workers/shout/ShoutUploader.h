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

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "src/helpers/AppSettings.h"
#include "src/net/HttpClient.h"
#include "src/workers/base/Message.h"

// Uploads shouts on its own thread, one at a time and in order
class ShoutUploader {
 public:
    // "onMessage" receives upload failures, on the uploader's thread
    ShoutUploader(AppSettings::ConnectivityInfos connectivityInfos, workers::MessageSink onMessage);

    // aborts the upload in progress, if any, and drops the queued shouts
    ~ShoutUploader();

    // queues the shout (JSON) for upload
    void upload(std::string shoutAsJson);

    // waits until all queued shouts are uploaded, or "timeout" : false if some remain
    bool waitForDrain(std::chrono::milliseconds timeout);

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    const workers::MessageSink _onMessage;
    net::HttpClient _client;

    std::mutex _mutex;
    std::condition_variable_any _queueChanged;
    std::deque<std::string> _queue;
    bool _uploading = false;

    // declared last : started once the members it uses are initialized, stopped and joined first
    std::jthread _worker;

    void _uploadQueuedShouts(std::stop_token stop);
};

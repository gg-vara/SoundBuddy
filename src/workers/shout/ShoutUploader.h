// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "src/settings/AppSettings.h"
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

    void _uploadQueuedShouts(const std::stop_token& stop);
};

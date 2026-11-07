// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

#include "src/settings/AppSettings.h"
#include "src/workers/base/Message.h"

// Exports the music library (JSON, compressed), then uploads it to the platform, on its own thread
class FeederThread {
 public:
    // called on the feeder's thread
    struct Callbacks {
        workers::MessageSink onMessage;
        std::function<void()> onFilesGenerated;  // library export and warnings written
        std::function<void()> onFinished;
    };

    // starts right away
    FeederThread(AppSettings::ConnectivityInfos connectivityInfos, Callbacks callbacks);

    // stops : an upload in progress is aborted, a library export in progress finishes first
    ~FeederThread();

    bool isRunning() const;

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    const Callbacks _callbacks;
    std::atomic<bool> _running { true };

    // declared last : started once the members it uses are initialized, stopped and joined first
    std::jthread _worker;

    void _run(const std::stop_token& stop);
    void _feed(const std::stop_token& stop);
    void _say(std::string text, workers::MessageKind kind = workers::MessageKind::Standard);
};

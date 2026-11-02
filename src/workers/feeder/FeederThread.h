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

#include <atomic>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

#include "src/helpers/AppSettings.hpp"
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

    void _run(std::stop_token stop);
    void _feed(std::stop_token stop);
    void _say(std::string text, workers::MessageKind kind = workers::MessageKind::Standard);
};

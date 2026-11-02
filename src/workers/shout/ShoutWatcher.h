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

#include "src/workers/base/Message.h"

#include "ShoutPayload.h"
#include "ShoutPayloadConsumer.h"

// Watches what the music app plays, on its own thread : waits for the app, then reports what it plays
// on every meaningful change (a "shout"), session after session
class ShoutWatcher {
 public:
    // called on the watcher's thread
    struct Callbacks {
        workers::MessageSink onMessage;
        std::function<void(std::string shoutAsJson)> onShout;
        std::function<void()> onFinished;
    };

    // starts right away
    explicit ShoutWatcher(Callbacks callbacks);

    // stops right away, while waiting for the music app as during a session
    ~ShoutWatcher();

    bool isRunning() const;

 private:
    const Callbacks _callbacks;
    ShoutPayloadConsumer _consumer;
    std::atomic<bool> _running { true };

    #ifdef _WIN32
        // music app process of the previous session
        unsigned long _previousProcessID = 0;
    #endif

    // declared last : started once the members it uses are initialized, stopped and joined first
    std::jthread _worker;

    void _run(std::stop_token stop);
    void _say(std::string text, workers::MessageKind kind = workers::MessageKind::Standard);
    void _processPayload(const ShoutPayload &incomingPayload);

    // platform specific

    // waits for a session to be available : false once a stop is requested
    bool _waitForAvailableSession(std::stop_token stop);

    // watches until the session ends or a stop is requested
    void _execWatchSession(std::stop_token stop);
};

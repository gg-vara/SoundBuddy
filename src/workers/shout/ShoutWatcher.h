// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

    void _run(const std::stop_token& stop);
    void _say(std::string text, workers::MessageKind kind = workers::MessageKind::Standard);
    void _processPayload(const ShoutPayload& incomingPayload);

    // platform specific

    // waits for a session to be available : false once a stop is requested
    bool _waitForAvailableSession(const std::stop_token& stop);

    // watches until the session ends or a stop is requested
    void _execWatchSession(const std::stop_token& stop);
};

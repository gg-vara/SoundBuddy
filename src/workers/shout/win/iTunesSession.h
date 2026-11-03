// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef _WIN32

#pragma once

#include <functional>
#include <stop_token>
#include <string>

#include "iTunesCOMClient.h"

// Watches what iTunes plays, without any framework dependency : reports a snapshot when connected,
// on every play / stop event, and when the user jumps within the current track (which triggers no event).
// Must be created and run on a single thread, initialized as a COM STA.
class iTunesSession {
 public:
    using OnSnapshot = std::function<void(const iTunesCOMClient::Snapshot&)>;

    // throws std::runtime_error if iTunes cannot be reached
    iTunesSession();

    // blocks until iTunes is about to quit, or a stop is requested on "stop" (which wakes it up right away)
    void run(const std::stop_token& stop, const OnSnapshot& onSnapshot);

 private:
    static constexpr const unsigned int pollingIntervalMs = 1000;

    // position difference between 2 pollings beyond which the user jumped within the track
    static constexpr const int jumpThresholdMs = 2000;

    // flagged by events, handled outside of their callback to avoid reentrant calls into iTunes
    bool _playerStateChanged = false;
    bool _quitting = false;

    // declared after the flags its events callback writes to
    iTunesCOMClient _client;

    std::wstring _lastLocation;
    int _lastPositionMS = 0;

    bool _hasJumped();
    static void _dispatchMessages();
};

#endif

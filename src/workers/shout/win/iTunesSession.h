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

#ifdef _WIN32

#pragma once

#include <atomic>
#include <functional>
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

    // blocks until iTunes is about to quit, or "keepRunning" becomes false (checked at least every second)
    void run(const std::atomic<bool>& keepRunning, const OnSnapshot& onSnapshot);

 private:
    static constexpr const unsigned int _pollingIntervalMS = 1000;

    // position difference between 2 pollings beyond which the user jumped within the track
    static constexpr const int _jumpThresholdMS = 2000;

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

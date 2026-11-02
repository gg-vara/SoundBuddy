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

#include "iTunesSession.h"

#include <cstdlib>

iTunesSession::iTunesSession() :
    _client([this](iTunesCOMClient::Event event) {
        if (event == iTunesCOMClient::Event::AboutToQuit) {
            _quitting = true;
        } else {
            _playerStateChanged = true;
        }
    }) {}

void iTunesSession::run(std::stop_token stop, const OnSnapshot& onSnapshot) {
    // a stop wakes the wait for messages up, with an empty message to this thread
    const auto threadId = GetCurrentThreadId();
    std::stop_callback wakeUp(stop, [threadId] { PostThreadMessageW(threadId, WM_NULL, 0, 0); });

    onSnapshot(_client.snapshot());

    auto nextPolling = GetTickCount64() + _pollingIntervalMS;

    while (!stop.stop_requested() && !_quitting) {
        // events may also be delivered while calling iTunes, so they are checked on every iteration
        if (_playerStateChanged) {
            _playerStateChanged = false;
            onSnapshot(_client.snapshot());
            continue;
        }

        // wait for messages (which deliver events) or the next polling
        const auto now = GetTickCount64();
        const auto wait = static_cast<DWORD>(nextPolling > now ? nextPolling - now : 0);

        if (MsgWaitForMultipleObjectsEx(0, nullptr, wait, QS_ALLINPUT, MWMO_INPUTAVAILABLE) != WAIT_TIMEOUT) {
            _dispatchMessages();
            continue;
        }

        nextPolling = GetTickCount64() + _pollingIntervalMS;

        if (_hasJumped()) {
            onSnapshot(_client.snapshot());
        }
    }
}

bool iTunesSession::_hasJumped() {
    const auto position = _client.playingPosition();
    if (position.location.empty()) return false;

    // another track : positions cannot be compared
    if (position.location != _lastLocation) {
        _lastLocation = position.location;
        _lastPositionMS = position.positionMS;
        return false;
    }

    const auto jumped = std::abs(_lastPositionMS - position.positionMS) > _jumpThresholdMS;
    _lastPositionMS = position.positionMS;

    return jumped;
}

void iTunesSession::_dispatchMessages() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

#endif

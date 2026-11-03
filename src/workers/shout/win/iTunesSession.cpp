// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stop_token>
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

void iTunesSession::run(const std::stop_token& stop, const OnSnapshot& onSnapshot) {
    // a stop wakes the wait for messages up, with an empty message to this thread
    const auto threadId = GetCurrentThreadId();
    const std::stop_callback wakeUp(stop, [threadId] { PostThreadMessageW(threadId, WM_NULL, 0, 0); });

    onSnapshot(_client.snapshot());

    auto nextPolling = GetTickCount64() + pollingIntervalMs;

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

        nextPolling = GetTickCount64() + pollingIntervalMs;

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

    const auto jumped = std::abs(_lastPositionMS - position.positionMS) > jumpThresholdMs;
    _lastPositionMS = position.positionMS;

    return jumped;
}

void iTunesSession::_dispatchMessages() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

#endif

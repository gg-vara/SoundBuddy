// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ShoutWatcher.h"

#include "src/helpers/Platform.h"
#include "src/i18n/Translations.h"
#include "src/workers/base/Message.h"
#include "src/workers/base/Wait.h"

#include <chrono>
#include <windows.h>
#include <combaseapi.h>

#include <exception>
#include <stop_token>

#include "src/workers/shout/ShoutPayload.h"
#include "src/workers/shout/win/iTunesCOMClient.h"
#include "win/iTunesSession.h"

//
bool ShoutWatcher::_waitForAvailableSession(const std::stop_token& stop) {
    using namespace std::chrono_literals;
    const auto windowName = platform::widen(platform::musicAppName());

    while (!stop.stop_requested()) {
        // search for music app...
        auto* windowHandler = FindWindowW(nullptr, windowName.c_str());

        // if not found, wait and retry
        if (!windowHandler) {
            workers::waitFor(stop, 1s);
            continue;
        }

        // Music App found, store the associated PID
        DWORD currentProcessID = 0;
        const auto success = GetWindowThreadProcessId(windowHandler, &currentProcessID);

        // if cannot fetch, wait and retry
        if (success == 0) {
            workers::waitFor(stop, 1s);
            continue;
        }

        // if has already been ran and iTunes is still the same instance, do not proceed, since it means that the window is shutting down
        // right now.
        if (_previousProcessID != 0 && _previousProcessID == currentProcessID) {
            workers::waitFor(stop, 1s);
            continue;
        }

        // store process ID
        _previousProcessID = currentProcessID;

        //
        return !stop.stop_requested();
    };

    //
    return false;
}

namespace {

ShoutPayload toPayload(const iTunesCOMClient::Snapshot& snapshot) {
    const auto& track = snapshot.track;

    // no current track, shout empty
    if (!track.exists) return ShoutPayload {};

    ShoutPayload payload {};
    payload.name = platform::narrow(track.name);
    payload.album = platform::narrow(track.album);
    payload.artist = platform::narrow(track.artist);
    payload.genre = platform::narrow(track.genre);
    payload.duration = track.duration;
    payload.playedDate = platform::narrow(track.playedDate);
    payload.skippedDate = platform::narrow(track.skippedDate);
    payload.year = track.year;
    payload.fileLocation = platform::narrow(track.location);
    payload.positionMs = snapshot.positionMS;
    payload.playing = snapshot.playing;

    return payload;
}

}  // namespace

void ShoutWatcher::_execWatchSession(const std::stop_token& stop) {
    // log..
    _say(i18n::format(i18n::tr("Connecting to %1 ..."), { platform::musicAppName() }));

    // initiate COM
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    try {
        // log..
        _say(std::string { i18n::tr("Initializing communication...") });

        iTunesSession session;

        // log..
        _say(i18n::format(i18n::tr("Listening to %1 !"), { platform::musicAppName() }));

        // process events
        session.run(stop, [this](const iTunesCOMClient::Snapshot& snapshot) { _processPayload(toPayload(snapshot)); });
    } catch (const std::exception& e) {
        _say(e.what(), workers::MessageKind::Issue);
    }

    // uninitialize COM
    CoUninitialize();
}

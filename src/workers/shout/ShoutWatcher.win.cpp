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

#include "ShoutWatcher.h"

#include "src/helpers/Platform.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/workers/base/Wait.h"

#include <windows.h>
#include <combaseapi.h>

#include <exception>

#include "win/iTunesSession.h"

//
bool ShoutWatcher::_waitForAvailableSession(std::stop_token stop) {
    using namespace std::chrono_literals;
    const auto windowName = platform::widen(platform::musicAppName());

    while (!stop.stop_requested()) {
        // search for music app...
        auto windowHandler = FindWindowW(nullptr, windowName.c_str());

        // if not found, wait and retry
        if(!windowHandler) {
            workers::waitFor(stop, 1s);
            continue;
        }

        // Music App found, store the associated PID
        DWORD currentProcessID;
        auto success = GetWindowThreadProcessId(windowHandler, &currentProcessID);
        
        // if cannot fetch, wait and retry
        if(!success) {
            workers::waitFor(stop, 1s);
            continue;
        }

        // if has already been ran and iTunes is still the same instance, do not proceed, since it means that the window is shutting down right now.
        if(_previousProcessID != 0 && _previousProcessID == currentProcessID) {
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
    const auto &track = snapshot.track;

    // no current track, shout empty
    if (!track.exists) return ShoutPayload{};

    ShoutPayload payload{};
    payload.tName = platform::narrow(track.name);
    payload.tAlbum = platform::narrow(track.album);
    payload.tArtist = platform::narrow(track.artist);
    payload.tGenre = platform::narrow(track.genre);
    payload.iDuration = track.duration;
    payload.tDatePlayed = platform::narrow(track.playedDate);
    payload.tDateSkipped = platform::narrow(track.skippedDate);
    payload.tYear = track.year;
    payload.tFileLocation = platform::narrow(track.location);
    payload.iPlayerPosMS = snapshot.positionMS;
    payload.iPlayerState = snapshot.playing;

    return payload;
}

}  // namespace

void ShoutWatcher::_execWatchSession(std::stop_token stop) {
    // log..
    _say(i18n::format(i18n::tr("Connecting to %1 ..."), { platform::musicAppName() }));

    // initiate COM
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    try {
        // log..
        _say(std::string { i18n::tr("Initializing communication...") });

        iTunesSession session;

        // log..
        _say(i18n::format(i18n::tr("Listening to %1 !"), { platform::musicAppName() }));

        // process events
        session.run(stop, [this](const iTunesCOMClient::Snapshot& snapshot) {
            _processPayload(toPayload(snapshot));
        });
    } catch (const std::exception& e) {
        _say(e.what(), workers::MessageKind::Issue);
    }

    // uninitialize COM
    CoUninitialize();
}

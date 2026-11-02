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
#include "src/i18n/trad.hpp"
#include "src/logs/Logs.h"

#include <windows.h>
#include <combaseapi.h>

#include <exception>

#include "win/iTunesSession.h"

//
bool ShoutWatcher::_waitForAvailableSession() {
    while (this->_keepWatching) {
        // search for music app...
        auto windowHandler = FindWindowA(0, musicAppName().toUtf8());

        // if not found, wait and retry
        if(!windowHandler) {
            this->sleep(1);
            continue;
        }

        // Music App found, store the associated PID
        DWORD currentProcessID;
        auto success = GetWindowThreadProcessId(windowHandler, &currentProcessID);
        
        // if cannot fetch, wait and retry
        if(!success) {
            this->sleep(1);
            continue;
        }

        // if has already been ran and iTunes is still the same instance, do not proceed, since it means that the window is shutting down right now.
        if(this->_oldProcessID != NULL && this->_oldProcessID == currentProcessID) {
            this->sleep(1);
            continue;
        }

        // store process ID
        this->_oldProcessID = currentProcessID;

        //
        return this->_keepWatching;
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

void ShoutWatcher::_execWatchSession() {
    // log..
    emit forwardMessage(
        tr("Connecting to %1 ...")
            .arg(musicAppName())
    );

    // initiate COM
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    try {
        // log..
        emit forwardMessage(tr("Initializing communication..."));

        iTunesSession session;

        // log..
        emit forwardMessage(
            tr("Listening to %1 !")
                .arg(musicAppName())
        );

        // process events
        session.run(this->_keepWatching, [this](const iTunesCOMClient::Snapshot& snapshot) {
            this->processPayload(toPayload(snapshot));
        });
    } catch (const std::exception& e) {
        logs::error("ShoutWatcher", "iTunes session failed : {}", e.what());
        emit forwardMessage(QString::fromUtf8(e.what()), MessageType::ISSUE);
    }

    // uninitialize COM
    CoUninitialize();
}

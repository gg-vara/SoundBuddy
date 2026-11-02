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

#include <QFile>
#include <QProcess>

#include <chrono>
#include <string_view>

#include "src/helpers/Json.hpp"
#include "src/workers/base/Wait.h"

namespace {

std::string toString(yyjson_val* value) {
    return std::string { json::string(value) };
}

}  // namespace

// consider the session always available on OSX since we do not depend on the app beeing ran to request for shout states
bool ShoutWatcher::_waitForAvailableSession(const std::stop_token& stop) {
    return !stop.stop_requested();
}

void ShoutWatcher::_execWatchSession(const std::stop_token& stop) {
    // define applescript to get shout values
    const auto scriptContent = QFile(":/mac/CurrentlyPlaying.applescript").readAll();

    // prepare script exec
    QProcess p;
    p.setProgram("/usr/bin/osascript");
    p.setArguments({ "-l", "AppleScript", "-s", "s" });
    p.write(scriptContent);
    p.closeWriteChannel();

    //
    ShoutPayload payload;
    QByteArray result;
    bool processOK;

    // loop until user said not to
    while (!stop.stop_requested()) {
        // get shout results
        p.start();

        processOK = p.waitForReadyRead(200);
        if (!processOK || stop.stop_requested()) {
            goto maybeWait;
        }

        result = p.readAll();
        processOK = p.waitForFinished(100);
        if (!processOK || stop.stop_requested()) {
            goto maybeWait;
        }

        // default values and inst
        payload = ShoutPayload {};

        // if has result
        if (result.size()) {
            // turn results into array
            result[0] = '[';
            result[result.size() - 1] = ']';

            // cast to json
            const auto doc = json::parse(std::string_view(result.constData(), static_cast<std::size_t>(result.size())));
            const auto trackData = json::root(doc);
            const auto at = [trackData](std::size_t index) { return yyjson_arr_get(trackData, index); };

            // get values for shout
            payload.fileLocation = toString(at(0));
            payload.name = toString(at(1));
            payload.album = toString(at(2));
            payload.artist = toString(at(3));
            payload.genre = toString(at(4));
            payload.duration = static_cast<int>(json::integer(at(5)));
            payload.year = static_cast<int>(json::integer(at(6)));
            payload.positionMs =
                static_cast<int>(json::integer(at(7)) * 1000);  // TODO get the real thing instead of emulating the millisecond track state
            payload.playing = json::string(at(8)) == "paused" ? 0 : 1;
            payload.skippedDate = toString(at(9));
            payload.playedDate = toString(at(10));
        }

        // compare with old shout, if equivalent, don't reshout
        _processPayload(payload);

    // maybe wait a bit before looping again
    maybeWait:
        workers::waitFor(stop, std::chrono::seconds { 1 });
    }
}

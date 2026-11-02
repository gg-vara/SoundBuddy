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

#include <utility>

#include "src/helpers/Platform.h"
#include "src/helpers/Timestamps.hpp"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"

ShoutWatcher::ShoutWatcher(Callbacks callbacks) :
    _callbacks(std::move(callbacks)),
    _worker([this](std::stop_token stop) { _run(stop); }) {}

// the worker thread, destroyed first, is asked to stop (which ends the session in progress) and joined
ShoutWatcher::~ShoutWatcher() = default;

bool ShoutWatcher::isRunning() const {
    return _running;
}

void ShoutWatcher::_say(std::string text, workers::MessageKind kind) {
    workers::report(_callbacks.onMessage, "ShoutWatcher", { std::move(text), kind });
}

// runs on the worker thread
void ShoutWatcher::_run(std::stop_token stop) {
    const auto musicApp = platform::musicAppName();

    _say(i18n::format(i18n::tr("Waiting for %1 to launch..."), { musicApp }));

    while (_waitForAvailableSession(stop)) {
        _say(i18n::format(i18n::tr("%1 is ready, watching session starts."), { musicApp }));

        _execWatchSession(stop);

        _say(i18n::format(i18n::tr("Stopped watching %1, ending session."), { musicApp }));

        // since session ended, tell that we do not listen to anything anymore
        _processPayload(ShoutPayload {});

        // if session ended but we still want to watch, tell the user
        if (!stop.stop_requested()) {
            _say(i18n::format(i18n::tr("Waiting for %1 to launch again..."), { musicApp }));
        }
    }

    logs::info("ShoutWatcher", "Stopped watching");

    _running = false;
    if (_callbacks.onFinished) _callbacks.onFinished();
}

void ShoutWatcher::_processPayload(const ShoutPayload &incomingPayload) {
    auto output = _consumer.consume(incomingPayload);
    if (!output.hasMeaningfulChange) return;

    if (_callbacks.onShout) _callbacks.onShout(std::move(output.parsingResult.json));

    if (output.parsingResult.isEmpty) {
        _say(i18n::format(i18n::tr("%1: Shouting -> Nothing"), { timestamps::local() }));
        return;
    }

    const auto playerState = incomingPayload.iPlayerState ? i18n::tr("playing") : i18n::tr("paused");
    _say(i18n::format(i18n::tr("%1: Shouting -> %2 - %3 - %4 (%5)"), {
        timestamps::local(),
        incomingPayload.tName,
        incomingPayload.tAlbum,
        incomingPayload.tArtist,
        playerState
    }));
}

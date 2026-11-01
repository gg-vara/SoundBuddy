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

#include <functional>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimer>
#include <QEventLoop>

#include "src/i18n/trad.hpp"

ShoutWatcher::ShoutWatcher() {}

void ShoutWatcher::quit() {
    // on Windows, the watch session notices it within a second
    this->_keepWatching = false;

    MessengerThread::quit();
}

void ShoutWatcher::run() {
    //
    emit forwardMessage(
        tr("Waiting for %1 to launch...")
            .arg(musicAppName())
    );

    qDebug() << "[ShoutWatcher] Start watching.";

    //
    while(this->_waitForAvailableSession()) {
        //
        auto msg = tr("%1 is ready, watching session starts.").arg(musicAppName());
        emit forwardMessage(msg);
        qDebug() << "[ShoutWatcher]" << msg;

        //
        this->_execWatchSession();

        //
        msg = tr("Stopped watching %1, ending session.").arg(musicAppName());
        emit forwardMessage(msg);
        qDebug() << "[ShoutWatcher]" << msg;

        // since session ended, tell that we do not listen to anything anymore
        this->processPayload(ShoutPayload{});

        // if session ended but we still want to watch, tell the user
        if(this->_keepWatching) {
            msg = tr("Waiting for %1 to launch again...").arg(musicAppName());
            emit forwardMessage(msg);
            qDebug() << "[ShoutWatcher]" << msg;
        }
    }

    //
    qDebug() << "[ShoutWatcher] Ending watch.";

    //
    this->exec();
}

void ShoutWatcher::processPayload(const ShoutPayload &incomingPayload) {
    //
    const auto output = this->_consumer.consume(incomingPayload);

    //
    if (output.hasMeaningfulChange) {
        //
        emit shoutReadyToSend(output.parsingResult.json);
        qDebug() << "[ShoutWatcher] Forwarding shout to uploader";

        //
        if (output.parsingResult.isEmpty) {
            //
            emit forwardMessage(
                tr("%1: Shouting -> Nothing")
                    .arg(QDateTime::currentDateTime().toString())
            );
        } else {
            // log content
            const auto pState = incomingPayload.iPlayerState ? tr("playing") : tr("paused");
            const auto logMessage =
                tr("%1: Shouting -> %2 - %3 - %4 (%5)")
                    .arg(QDateTime::currentDateTime().toString())
                    .arg(incomingPayload.tName)
                    .arg(incomingPayload.tAlbum)
                    .arg(incomingPayload.tArtist)
                    .arg(pState);
            
            //
            emit forwardMessage(logMessage);
        }
    }
}

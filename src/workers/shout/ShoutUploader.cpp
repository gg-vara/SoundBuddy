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


#include "ShoutUploader.h"

#include <QDebug>

#include <chrono>

#include "src/helpers/UploadHelper.hpp"

#include "version.h"

ShoutUploader::ShoutUploader(const AppSettings::ConnectivityInfos connectivityInfos) :
    _connectivityInfos(connectivityInfos),
    _client(APP_NAME "/" APP_CURRENT_VERSION),
    _worker(&ShoutUploader::_uploadQueuedShouts, this) {}

ShoutUploader::~ShoutUploader() {
    {
        std::lock_guard lock { _mutex };
        _stopping = true;
    }

    _queueChanged.notify_all();
    _client.cancel();
    _worker.join();
}

//
void ShoutUploader::waitForDrain(unsigned long timeout) {
    std::unique_lock lock { _mutex };

    const auto drained = _queueChanged.wait_for(lock, std::chrono::milliseconds { timeout }, [this] {
        return _queue.empty() && !_uploading;
    });

    if (drained) {
        qDebug() << "[ShoutUploader] Successfully drained.";
    } else {
        qDebug() << "[ShoutUploader] Drain timed out, remaining:" << _queue.size() + (_uploading ? 1 : 0);
    }
}

//
void ShoutUploader::uploadAsShout(const std::string &shoutAsJson) {
    //
    qDebug() << "[ShoutUploader] Shout received, uploading...";

    {
        std::lock_guard lock { _mutex };
        _queue.push_back(shoutAsJson);
    }

    _queueChanged.notify_all();
}

// runs on the worker thread
void ShoutUploader::_uploadQueuedShouts() {
    while (true) {
        std::string shout;

        {
            std::unique_lock lock { _mutex };
            _queueChanged.wait(lock, [this] { return _stopping || !_queue.empty(); });
            if (_stopping) return;

            shout = std::move(_queue.front());
            _queue.pop_front();
            _uploading = true;
        }

        //
        const auto uploadInfos = AppSettings::getShoutUploadInfos();
        const auto response = UploadHelper::uploadDataToPlatform(_client, {
            _connectivityInfos,
            uploadInfos,
            shout
        }, false);

        if (response.ok()) {
            qDebug() << "[ShoutUploader] Uploaded with HTTP code " << response.status;
        } else {
            emit forwardMessage(
                tr("An error occured while shouting tracks infos to %1 platform : %2")
                    .arg(DEST_PLATFORM_PRODUCT_NAME)
                    .arg(UploadHelper::describeFailure(response)),
                MessageType::ISSUE
            );
        }

        {
            std::lock_guard lock { _mutex };
            _uploading = false;
        }

        _queueChanged.notify_all();
    }
}

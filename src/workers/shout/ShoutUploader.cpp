// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ShoutUploader.h"

#include <chrono>
#include <mutex>
#include <stop_token>
#include <string>
#include <utility>

#include "src/settings/AppSettings.h"
#include "src/net/UploadHelper.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"

#include "src/workers/base/Message.h"
#include "version.h"

ShoutUploader::ShoutUploader(AppSettings::ConnectivityInfos connectivityInfos, workers::MessageSink onMessage) :
    _connectivityInfos(std::move(connectivityInfos)),
    _onMessage(std::move(onMessage)),
    _client(APP_NAME "/" APP_CURRENT_VERSION),
    _worker([this](const std::stop_token& stop) { _uploadQueuedShouts(stop); }) {}

// the worker thread, destroyed first, is asked to stop (which aborts the upload in progress) and joined
ShoutUploader::~ShoutUploader() = default;

bool ShoutUploader::waitForDrain(std::chrono::milliseconds timeout) {
    std::unique_lock lock { _mutex };

    const auto drained = _queueChanged.wait_for(lock, timeout, [this] { return _queue.empty() && !_uploading; });

    if (drained) {
        logs::debug("ShoutUploader", "Successfully drained");
    } else {
        logs::warning("ShoutUploader", "Drain timed out, {} shout(s) not uploaded", _queue.size() + (_uploading ? 1 : 0));
    }

    return drained;
}

void ShoutUploader::upload(std::string shoutAsJson) {
    logs::debug("ShoutUploader", "Shout received, uploading");

    {
        const std::scoped_lock lock { _mutex };
        _queue.push_back(std::move(shoutAsJson));
    }

    _queueChanged.notify_all();
}

// runs on the worker thread
void ShoutUploader::_uploadQueuedShouts(const std::stop_token& stop) {
    while (true) {
        std::string shout;

        {
            std::unique_lock lock { _mutex };

            // false once a stop is requested
            if (!_queueChanged.wait(lock, stop, [this] { return !_queue.empty(); })) return;

            shout = std::move(_queue.front());
            _queue.pop_front();
            _uploading = true;
        }

        //
        const auto response = UploadHelper::uploadShout(_client, _connectivityInfos, shout, stop);

        if (response.ok()) {
            logs::debug("ShoutUploader", "Uploaded, HTTP {}", response.status);
        } else if (!stop.stop_requested()) {
            workers::report(
                _onMessage, "ShoutUploader",
                { .text = i18n::format(
                      i18n::tr("An error occured while shouting tracks infos to %1 platform : %2"),
                      { DEST_PLATFORM_PRODUCT_NAME, UploadHelper::describeFailure(response) }
                  ),
                  .kind = workers::MessageKind::Issue }
            );
        }

        {
            const std::scoped_lock lock { _mutex };
            _uploading = false;
        }

        _queueChanged.notify_all();
    }
}

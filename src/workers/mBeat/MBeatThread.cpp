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

#include "MBeatThread.h"

#include <format>
#include <stop_token>
#include <string>
#include <utility>

#include "src/helpers/AppSettings.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/net/HttpClient.h"
#include "src/net/Multipart.h"
#include "src/workers/base/Wait.h"

#include "version.h"

namespace mbeat {

std::string credentialsUrl(const AppSettings::ConnectivityInfos& connectivityInfos) {
    const auto url = connectivityInfos.getSoundHeartbeatUrl() + "/creds";

    const auto schemeEnd = url.find("://");
    const auto address = schemeEnd == std::string::npos ? url : url.substr(schemeEnd + 3);
    const auto host = address.substr(0, address.find_first_of(":/"));

    const auto wantsUnsecure = host == "localhost" || host == "127.0.0.1";
    return (wantsUnsecure ? "http://" : "https://") + address;
}

}  // namespace mbeat

MBeatThread::MBeatThread(AppSettings::ConnectivityInfos connectivityInfos, mbeat::OnStatus onStatus) :
    _connectivityInfos(std::move(connectivityInfos)),
    _onStatus(std::move(onStatus)),
    _client(APP_NAME "/" APP_CURRENT_VERSION),
    _worker([this](const std::stop_token& stop) { _run(stop); }) {}

// the worker thread, destroyed first, is asked to stop (which aborts the request in progress) and joined
MBeatThread::~MBeatThread() = default;

// runs on the worker thread
void MBeatThread::_run(const std::stop_token& stop) {
    const auto notify = [this](std::string text, mbeat::Connectivity connectivity) {
        if (_onStatus) _onStatus({ .text = std::move(text), .connectivity = connectivity });
    };

    logs::debug("MBeat", "Start");

    if (!_connectivityInfos.areOK) {
        notify(std::string { i18n::tr("Waiting for appropriate credentials.") }, mbeat::Connectivity::Refused);
        return;
    }

    const auto request = _createRequest();

    while (!stop.stop_requested()) {
        logs::debug("MBeat", "Checking credentials");
        notify(std::string { i18n::tr("Asking for credentials validation...") }, mbeat::Connectivity::Checking);

        const auto response = _client.send(request, stop);
        if (stop.stop_requested()) break;

        if (response.status != 200) {
            // body first, as the platform explains why credentials are refused
            std::string details = response.body;
            if (details.empty()) {
                details = response.error.empty() ? std::format("HTTP {}", response.status) : response.error;
            }

            logs::warning("MBeat", "Credentials check failed : {}", details);
            notify(
                i18n::format(i18n::tr("An error occured while connecting with %1 platform : %2"), { DEST_PLATFORM_PRODUCT_NAME, details }),
                mbeat::Connectivity::Refused
            );
        } else {
            logs::info("MBeat", "Logged as \"{}\"", _connectivityInfos.username);
            notify(i18n::format(i18n::tr("Logged as \"%1\""), { _connectivityInfos.username }), mbeat::Connectivity::Connected);
        }

        // if credentials are not accepted, no need to loop back again, user needs to update them.
        if (response.status == 403) break;

        // will retry to confirm
        logs::debug("MBeat", "Will retry soon");
        workers::waitFor(stop, heartbeatInterval);
    }

    logs::debug("MBeat", "Shutdown");
}

net::Request MBeatThread::_createRequest() const {
    net::Request request { .method = "POST", .url = mbeat::credentialsUrl(_connectivityInfos) };
    request.timeout = requestTimeout;  // setup an earlier timeout
    request.headers = {
        { .name = "Accept-Language", .value = std::string { i18n::languageCode() } },
        { .name = "X-Client-Id", .value = APP_NAME },
        { .name = "X-Client-Version", .value = APP_CURRENT_VERSION },
    };

    // credentials
    net::Multipart form;
    form.addField("username", _connectivityInfos.username);
    form.addField("password", _connectivityInfos.password);
    form.applyTo(request);

    return request;
}

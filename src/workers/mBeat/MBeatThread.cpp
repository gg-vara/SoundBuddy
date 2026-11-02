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
#include <string>

#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/net/Multipart.h"

#include "version.h"

MBeatThread::MBeatThread(const AppSettings::ConnectivityInfos &connectivityInfos) :
    _connectivityInfos(connectivityInfos),
    _client(APP_NAME "/" APP_CURRENT_VERSION) {}

void MBeatThread::run() {
    //
    logs::debug("MBeat", "Start");

    //
    // Check connectivity infos
    //

    if(!_connectivityInfos.areOK) {
        //
        emit updateConnectivityStatus(
            tr("Waiting for appropriate credentials."),
            ConnectivityIndicator::NOK
        );

        //
        return;
    }

    //
    const auto request = this->_createRequest();

    while (!this->_isStopping()) {
        //
        logs::debug("MBeat", "Checking credentials");
        emit updateConnectivityStatus(
            tr("Asking for credentials validation..."),
            ConnectivityIndicator::ONGOING
        );

        const auto response = _client.send(request);
        if (this->_isStopping()) break;

        //
        // handle status bar update
        //

        if (response.status != 200) {
            // body first, as the platform explains why credentials are refused
            std::string details = response.body;
            if (details.empty()) {
                details = response.error.empty()
                    ? std::format("HTTP {}", response.status)
                    : response.error;
            }

            //
            emit updateConnectivityStatus(
                tr("An error occured while connecting with %1 platform : %2")
                    .arg(DEST_PLATFORM_PRODUCT_NAME)
                    .arg(QString::fromStdString(details)),
                ConnectivityIndicator::NOK
            );

            //
            logs::warning("MBeat", "Credentials check failed : {}", details);

        } else {
            //
            emit updateConnectivityStatus(
                tr("Logged as \"%1\"")
                    .arg(QString::fromStdString(_connectivityInfos.username)),
                ConnectivityIndicator::OK
            );

            //
            logs::info("MBeat", "Logged as \"{}\"", _connectivityInfos.username);
        }

        // if credentials are not accepted, no need to loop back again, user needs to update them.
        if (response.status == 403) break;

        // will retry to confirm
        logs::debug("MBeat", "Will retry soon");
        std::unique_lock lock { _mutex };
        _wakeUp.wait_for(lock, HEARTBEAT_INTERVAL, [this] { return _stopping; });
    }

    // loop has exited
    logs::debug("MBeat", "Shutdown");
}

void MBeatThread::quit() {
    {
        std::lock_guard lock { _mutex };
        _stopping = true;
    }

    _wakeUp.notify_all();
    _client.cancel();
}

bool MBeatThread::_isStopping() {
    std::lock_guard lock { _mutex };
    return _stopping;
}

namespace {

// "url" in HTTPS, or in HTTP for a platform running locally
std::string withPlatformScheme(const std::string& url) {
    const auto schemeEnd = url.find("://");
    const auto address = schemeEnd == std::string::npos ? url : url.substr(schemeEnd + 3);
    const auto host = address.substr(0, address.find_first_of(":/"));

    const auto wantsUnsecure = host == "localhost" || host == "127.0.0.1";
    return (wantsUnsecure ? "http://" : "https://") + address;
}

}  // namespace

net::Request MBeatThread::_createRequest() const {
    // build request
    net::Request request { "POST", withPlatformScheme(this->_connectivityInfos.getSoundHeartbeatUrl() + "/creds") };
    request.timeout = REQUEST_TIMEOUT;  // setup an earlier timeout
    request.headers = {
        { "Accept-Language", std::string { i18n::languageCode() } },
        { "X-Client-Id", APP_NAME },
        { "X-Client-Version", APP_CURRENT_VERSION },
    };

    // credentials
    net::Multipart form;
    form.addField("username", this->_connectivityInfos.username);
    form.addField("password", this->_connectivityInfos.password);
    form.applyTo(request);

    //
    return request;
}

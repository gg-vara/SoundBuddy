// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MBeatThread.h"

#include <format>
#include <stop_token>
#include <string>
#include <utility>

#include "src/helpers/AppSettings.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/net/HttpClient.h"
#include "src/net/PlatformSession.h"
#include "src/workers/base/Wait.h"

#include "version.h"

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

    net::Request request { .method = "GET", .url = _connectivityInfos.session->apiUrl() + "/api/v1/me" };
    request.timeout = requestTimeout;  // setup an earlier timeout
    request.headers = { { .name = "Accept-Language", .value = std::string { i18n::languageCode() } } };

    while (!stop.stop_requested()) {
        logs::debug("MBeat", "Checking credentials");
        notify(std::string { i18n::tr("Asking for credentials validation...") }, mbeat::Connectivity::Checking);

        const auto response = _connectivityInfos.session->send(_client, request, stop);
        if (stop.stop_requested()) break;

        if (response.status != 200) {
            // first, as the platform explains why credentials are refused
            std::string details = net::PlatformSession::explain(response);
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

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

#include "src/helpers/AppSettings.h"
#include "src/net/HttpClient.h"

namespace mbeat {

enum class Connectivity {
    Refused,  // no credentials, refused ones, or platform unreachable
    Checking,
    Connected
};

struct Status {
    std::string text;  // UTF-8, translated
    Connectivity connectivity;
};

// receives the connectivity status, on the heartbeat's thread
using OnStatus = std::function<void(const Status&)>;

// URL checking the credentials : in HTTPS, or in HTTP for a platform running locally
std::string credentialsUrl(const AppSettings::ConnectivityInfos& connectivityInfos);

}  // namespace mbeat

// Periodically checks the platform credentials on its own thread, from construction until
// destruction, or until the credentials are refused
class MBeatThread {
 public:
    MBeatThread(AppSettings::ConnectivityInfos connectivityInfos, mbeat::OnStatus onStatus);

    // stops right away, even while waiting for the next heartbeat or a response
    ~MBeatThread();

    // heartbeats
    static inline const std::chrono::milliseconds heartbeatInterval { 30000 };
    static inline const std::chrono::milliseconds requestTimeout { 3000 };

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    const mbeat::OnStatus _onStatus;
    net::HttpClient _client;

    // declared last : started once the members it uses are initialized, stopped and joined first
    std::jthread _worker;

    void _run(const std::stop_token& stop);
    net::Request _createRequest() const;
};

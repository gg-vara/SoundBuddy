// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

#include "src/settings/AppSettings.h"
#include "src/net/HttpClient.h"

namespace heartbeat {

enum class Connectivity {
    Refused,      // no credentials, or refused ones : the user has something to do
    Unreachable,  // the platform does not answer, or fails : it is tried again
    Checking,
    Connected
};

struct Status {
    std::string text;  // UTF-8, translated
    Connectivity connectivity;
};

// receives the connectivity status, on the heartbeat's thread
using OnStatus = std::function<void(const Status&)>;

}  // namespace heartbeat

// Periodically checks the platform credentials on its own thread, from construction until
// destruction, or until the credentials are refused
class HeartbeatThread {
 public:
    HeartbeatThread(AppSettings::ConnectivityInfos connectivityInfos, heartbeat::OnStatus onStatus);

    // stops right away, even while waiting for the next heartbeat or a response
    ~HeartbeatThread();

    // heartbeats
    static inline const std::chrono::milliseconds heartbeatInterval { 30000 };
    static inline const std::chrono::milliseconds requestTimeout { 3000 };

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    const heartbeat::OnStatus _onStatus;
    net::HttpClient _client;

    // declared last : started once the members it uses are initialized, stopped and joined first
    std::jthread _worker;

    void _run(const std::stop_token& stop);
};

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

#pragma once

#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

#include "src/helpers/AppSettings.hpp"
#include "src/net/HttpClient.h"

namespace mbeat {

enum class Connectivity {
    Refused,   // no credentials, refused ones, or platform unreachable
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
    static inline const std::chrono::milliseconds HEARTBEAT_INTERVAL { 30000 };
    static inline const std::chrono::milliseconds REQUEST_TIMEOUT { 3000 };

 private:
    const AppSettings::ConnectivityInfos _connectivityInfos;
    const mbeat::OnStatus _onStatus;
    net::HttpClient _client;

    // declared last : started once the members it uses are initialized, stopped and joined first
    std::jthread _worker;

    void _run(std::stop_token stop);
    net::Request _createRequest() const;
};

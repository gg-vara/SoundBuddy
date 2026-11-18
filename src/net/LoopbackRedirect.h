// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace net {

// Where the browser comes back once the user logged in (RFC 8252, 7.3) : a listener on 127.0.0.1, on a port the system picks,
// only reachable from this computer, for as long as this object lives
class LoopbackRedirect {
 public:
    LoopbackRedirect();
    ~LoopbackRedirect();

    LoopbackRedirect(const LoopbackRedirect&) = delete;
    void operator=(const LoopbackRedirect&) = delete;

    // false if the system gave no port
    bool isListening() const { return _port != 0; }

    // what the authorization request names : http://127.0.0.1:<port>/callback
    std::string redirectUri() const;

    // the query of the first request to /callback, which gets "page" (HTML) as answer. Other requests, as for a favicon, get a 404.
    // Nothing if "stop" is requested or "timeout" passes first
    std::optional<std::string> wait(std::string_view page, std::chrono::seconds timeout, const std::stop_token& stop) const;

    // the value of "name" in "query", decoded; empty if missing
    static std::string parameter(std::string_view query, std::string_view name);

 private:
    std::uintptr_t _socket;
    std::uint16_t _port = 0;
};

}  // namespace net

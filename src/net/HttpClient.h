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
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace net {

struct Header {
    std::string name;
    std::string value;
};

struct Request {
    std::string method = "GET";
    std::string url;  // absolute, http:// or https://
    std::vector<Header> headers;
    std::string body;

    // maximum time without any progress (connection, sending, receiving); zero for no limit, as for event streams
    std::chrono::milliseconds timeout { 30000 };
};

struct Response {
    int status = 0;  // HTTP status, 0 if no response was received
    std::vector<Header> headers;
    std::string body;  // empty when streamed
    std::string error;  // transport error (resolution, connection, TLS, timeout, cancellation...), empty on success

    bool ok() const { return error.empty() && status >= 200 && status < 300; }

    // case-insensitive lookup
    std::optional<std::string> header(std::string_view name) const;
};

// receives the body of a streamed response as it arrives; returning false stops reading
using OnBodyChunk = std::function<bool(std::string_view chunk)>;

// Blocking HTTP(S) client, relying on the platform network stack (WinHTTP, NSURLSession) :
// TLS certificates and proxy settings are the system ones.
// A client sends one request at a time. A stop requested on the request's token, or cancel(), aborts it from any thread.
class HttpClient {
 public:
    explicit HttpClient(std::string userAgent);
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    void operator=(const HttpClient&) = delete;

    // response body is fully read before returning; a stop requested on "stop", even before sending, aborts the request
    Response send(const Request& request, std::stop_token stop = {});

    // response body is given to "onChunk" as it arrives (event streams, large downloads)
    Response stream(const Request& request, const OnBodyChunk& onChunk, std::stop_token stop = {});

    // aborts the ongoing request, if any, which then returns with an error
    void cancel();

 private:
    class Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace net

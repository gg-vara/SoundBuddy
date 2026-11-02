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
#include <optional>
#include <string>
#include <string_view>

namespace net {

struct SseEvent {
    std::string type = "message";
    std::string data;
    std::string id;  // last event id, as of this event
};

// Parses a text/event-stream (Server-Sent Events, WHATWG HTML spec), fed with chunks of any size
class SseParser {
 public:
    using OnEvent = std::function<void(const SseEvent&)>;

    explicit SseParser(OnEvent onEvent) :
        _onEvent(std::move(onEvent)) {}

    void feed(std::string_view chunk);

    // to send back as Last-Event-ID header when reconnecting
    const std::string& lastEventId() const { return _lastEventId; }

    // reconnection delay requested by the server, if any
    std::optional<std::chrono::milliseconds> retry() const { return _retry; }

 private:
    OnEvent _onEvent;

    std::string _line;               // incomplete line, waiting for the next chunk
    bool _skipNextLineFeed = false;  // a line ended with '\r', possibly followed by '\n' in the next chunk
    bool _firstLine = true;          // to remove a leading byte order mark

    std::string _eventType;
    std::string _data;
    std::string _lastEventId;
    std::optional<std::chrono::milliseconds> _retry;

    void _processLine(std::string_view line);
    void _processField(std::string_view field, std::string_view value);
    void _dispatch();
};

}  // namespace net

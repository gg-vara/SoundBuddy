// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

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

#include "SseParser.h"

#include <algorithm>

namespace net {

void SseParser::feed(std::string_view chunk) {
    for (const char c : chunk) {
        // "\r\n" is a single line ending, even when split between chunks
        if (_skipNextLineFeed) {
            _skipNextLineFeed = false;
            if (c == '\n') continue;
        }

        if (c == '\r' || c == '\n') {
            _skipNextLineFeed = c == '\r';
            _processLine(_line);
            _line.clear();
        } else {
            _line += c;
        }
    }
}

void SseParser::_processLine(std::string_view line) {
    if (_firstLine) {
        _firstLine = false;
        static constexpr const std::string_view bom = "\xEF\xBB\xBF";
        if (line.substr(0, bom.size()) == bom) line.remove_prefix(bom.size());
    }

    // an empty line dispatches the event
    if (line.empty()) {
        _dispatch();
        return;
    }

    // comment
    if (line.front() == ':') return;

    const auto colon = line.find(':');
    if (colon == std::string_view::npos) {
        _processField(line, {});
        return;
    }

    auto value = line.substr(colon + 1);
    if (!value.empty() && value.front() == ' ') value.remove_prefix(1);

    _processField(line.substr(0, colon), value);
}

void SseParser::_processField(std::string_view field, std::string_view value) {
    if (field == "event") {
        _eventType = value;
    } else if (field == "data") {
        _data += value;
        _data += '\n';
    } else if (field == "id") {
        if (value.find('\0') == std::string_view::npos) _lastEventId = value;
    } else if (field == "retry") {
        const bool digitsOnly = !value.empty() && value.size() <= 12
            && std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; });
        if (digitsOnly) _retry = std::chrono::milliseconds { std::stoll(std::string { value }) };
    }
}

void SseParser::_dispatch() {
    // no data : the event is discarded, but its type is reset anyway
    if (_data.empty()) {
        _eventType.clear();
        return;
    }

    _data.pop_back();  // trailing '\n'

    SseEvent event;
    if (!_eventType.empty()) event.type = std::move(_eventType);
    event.data = std::move(_data);
    event.id = _lastEventId;

    _eventType.clear();
    _data.clear();

    _onEvent(event);
}

}  // namespace net

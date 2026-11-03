// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SseParser.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

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
        if (line.starts_with(bom)) line.remove_prefix(bom.size());
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
        const bool digitsOnly =
            !value.empty() && value.size() <= 12 && std::ranges::all_of(value, [](char c) { return c >= '0' && c <= '9'; });
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

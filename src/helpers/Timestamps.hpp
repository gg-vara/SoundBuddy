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

#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <string>

// Current date and time, formatted
namespace timestamps {

// UTC, ISO 8601 as JavaScript reads it : 2011-10-08T07:07:09Z
inline std::string utcISO() {
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

namespace detail {

inline std::string local(std::chrono::system_clock::time_point point) {
    const auto now = std::chrono::system_clock::to_time_t(point);

    std::tm time {};
#ifdef _WIN32
    if (localtime_s(&time, &now) != 0) return {};
#else
    if (!localtime_r(&now, &time)) return {};
#endif

    std::array<char, 32> text {};
    const auto length = std::strftime(text.data(), text.size(), "%Y-%m-%d %H:%M:%S", &time);
    return { text.data(), length };
}

}  // namespace detail

// local time, for the user : 2011-10-08 09:07:09
inline std::string local() {
    return detail::local(std::chrono::system_clock::now());
}

// local time with milliseconds, for log files : 2011-10-08 09:07:09.042
inline std::string localPrecise() {
    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    return std::format("{}.{:03}", detail::local(now), milliseconds);
}

}  // namespace timestamps

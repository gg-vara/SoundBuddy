// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

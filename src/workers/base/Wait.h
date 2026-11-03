// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace workers {

// waits "duration", unless a stop is requested on "stop" meanwhile, which wakes it up right away : false then
inline bool waitFor(const std::stop_token& stop, std::chrono::milliseconds duration) {
    std::mutex mutex;
    std::condition_variable_any wakeUp;

    std::unique_lock lock { mutex };
    wakeUp.wait_for(lock, stop, duration, [] { return false; });

    return !stop.stop_requested();
}

}  // namespace workers

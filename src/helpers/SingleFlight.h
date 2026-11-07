// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <utility>

// One slow action at a time, without holding "mutex" during it : what the mutex guards stays readable meanwhile.
// Waiting for one's turn can be stopped, so that whoever stops a thread does not end up waiting for the action of another
class SingleFlight {
 public:
    // takes turns, with "lock" (on the guarded mutex) held : false if stopped first
    bool waitForTurn(std::unique_lock<std::mutex>& lock, const std::stop_token& stop) {
        return _ended.wait(lock, stop, [this] { return !_flying; });
    }

    // what "action" returns, run with "lock" (just used to take turns) released meanwhile, and released once done
    template <typename Action>
    auto fly(std::unique_lock<std::mutex>& lock, Action&& action) {
        _flying = true;
        lock.unlock();

        const Landing landing { *this, *lock.mutex() };
        return std::forward<Action>(action)();
    }

 private:
    // however the action ends
    class Landing {
     public:
        Landing(SingleFlight& flight, std::mutex& mutex) :
            _flight(flight),
            _mutex(mutex) {}
        Landing(const Landing&) = delete;
        Landing(Landing&&) = delete;
        Landing& operator=(const Landing&) = delete;
        Landing& operator=(Landing&&) = delete;

        ~Landing() {
            {
                const std::scoped_lock lock { _mutex };
                _flight._flying = false;
            }
            _flight._ended.notify_all();
        }

     private:
        SingleFlight& _flight;
        std::mutex& _mutex;
    };

    std::condition_variable_any _ended;
    bool _flying = false;
};

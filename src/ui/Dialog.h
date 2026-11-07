// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <utility>

namespace ui {

// from the application thread : what "dialog" answers, a modal one that "post" runs on the user interface thread.
// "ifClosing" once "closing" is set, or the dialog dropped : the user interface then waits for this thread, and would never answer
template <typename Post>
bool dialogAnswer(Post post, const std::atomic<bool>& closing, bool ifClosing, std::function<bool()> dialog) {
    const auto answer = std::make_shared<std::promise<bool>>();
    auto future = answer->get_future();
    post([answer, dialog = std::move(dialog)] { answer->set_value(dialog()); });

    while (future.wait_for(std::chrono::milliseconds { 100 }) != std::future_status::ready) {
        if (closing) return ifClosing;
    }

    try {
        return future.get();
    } catch (const std::future_error&) {
        return ifClosing;
    }
}

}  // namespace ui

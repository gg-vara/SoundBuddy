// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AppThread.h"

#ifdef _WIN32
#include <objbase.h>
#endif

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <utility>

#include "Controller.h"
#include "Post.h"
#include "src/logs/Logs.h"

namespace ui {

namespace {

struct Tasks {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::function<void()>> queue;
    bool accepting = false;
};

Tasks& tasks() {
    static Tasks instance;
    return instance;
}

void postToQueue(std::function<void()> task) {
    auto& all = tasks();
    {
        const std::scoped_lock lock { all.mutex };
        if (!all.accepting) return;
        all.queue.push_back(std::move(task));
    }
    all.changed.notify_one();
}

void startAccepting() {
    auto& all = tasks();
    const std::scoped_lock lock { all.mutex };
    all.accepting = true;
}

void stopAccepting() {
    auto& all = tasks();
    {
        const std::scoped_lock lock { all.mutex };
        all.accepting = false;
        all.queue.clear();
    }
    all.changed.notify_all();
}

// none once tasks are no longer accepted
std::function<void()> nextTask() {
    auto& all = tasks();
    std::unique_lock lock { all.mutex };
    all.changed.wait(lock, [&all] { return !all.queue.empty() || !all.accepting; });
    if (all.queue.empty()) return {};

    auto task = std::move(all.queue.front());
    all.queue.pop_front();
    return task;
}

void runController(Controller*& pointer, Shell& shell) {
    Controller controller { shell };
    pointer = &controller;

    while (const auto task = nextTask()) task();

    // destroying it stops the workers, whose last tasks are dropped
}

// what main() does on the other thread, which would otherwise end the process without a word
void runGuarded(Controller*& pointer, Shell& shell) {
    try {
        runController(pointer, shell);
        return;
    } catch (const std::exception& e) {
        logs::error("App", "Fatal error : {}", e.what());
    } catch (...) {
        logs::error("App", "Fatal error : unknown exception");
    }

    stopAccepting();
    shell.quit();
}

}  // namespace

AppThread::AppThread(Shell& shell) {
    startAccepting();
    setPoster(&postToQueue);

    _thread = std::jthread([this, &shell] {
#ifdef _WIN32
        // opening links and folders goes through the shell, which expects it
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#endif

        runGuarded(_controller, shell);

#ifdef _WIN32
        CoUninitialize();
#endif
    });
}

AppThread::~AppThread() {
    stopAccepting();
}

void AppThread::run(std::function<void(Controller&)> task) {
    post([this, task = std::move(task)] { task(*_controller); });
}

}  // namespace ui

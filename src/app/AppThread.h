// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <thread>

namespace ui {

class Controller;
class Shell;

// Where the application lives, apart from the user interface : the Controller is created, runs its tasks and is destroyed there.
// What takes time there, as stopping a worker, leaves the window responsive. post() reaches it
class AppThread {
 public:
    // "shell" outlives this, and is called from that thread. On a failure it logs it, and quits through "shell"
    explicit AppThread(Shell& shell);

    // drops what is left to run, then stops the workers and waits for them
    ~AppThread();

    AppThread(const AppThread&) = delete;
    void operator=(const AppThread&) = delete;

    // "task" later on that thread, after those asked before
    void run(std::function<void(Controller&)> task);

 private:
    Controller* _controller = nullptr;  // only touched on that thread

    // declared last : started once the members it uses are initialized
    std::jthread _thread;
};

}  // namespace ui

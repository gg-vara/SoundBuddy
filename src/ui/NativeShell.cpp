// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "NativeShell.h"

#include <cassert>
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

#include "src/app/AppThread.h"
#include "src/app/Controller.h"
#include "src/app/PageMessages.h"

namespace ui {

//
// From the application thread
//

void NativeShell::sendToPage(const std::string& event) {
    assert(AppThread::isCurrent());

    // without a page, it is in the state the next one gets : what is sent before that state is computed comes before its answer, and what
    // is sent after it comes after, as all of it is handed over in order
    _postToUi([this, event] {
        if (_pageLoaded) _evalNow("sb.receive(" + event + ");");
    });
}

void NativeShell::setTrayState(TrayState state) {
    assert(AppThread::isCurrent());

    _postToUi([this, state] {
        _lastCanOpenPlatform = state.canOpenPlatform;
        _applyTrayState(state);
    });
}

bool NativeShell::ask(std::string_view title, std::string_view question, Answer byDefault) {
    return _dialogAnswer(byDefault == Answer::Yes, [this, title = std::string { title }, question = std::string { question }, byDefault] {
        return _askNow(title, question, byDefault);
    });
}

void NativeShell::tell(std::string_view title, std::string_view text, Severity severity) {
    _dialogAnswer(/*ifClosing=*/true, [this, title = std::string { title }, text = std::string { text }, severity] {
        _tellNow(title, text, severity);
        return true;
    });
}

void NativeShell::quit(int exitCode) {
    assert(AppThread::isCurrent());

    _postToUi([this, exitCode] { _quitNow(exitCode); });
}

// "ifClosing" once closing : the user interface then waits for this thread, and would never answer
bool NativeShell::_dialogAnswer(bool ifClosing, std::function<bool()> dialog) {
    // from the user interface thread, it would wait for itself
    assert(AppThread::isCurrent());

    const auto answer = std::make_shared<std::promise<bool>>();
    auto future = answer->get_future();
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) : false positive, the task owns what it holds
    _postToUi([answer, dialog = std::move(dialog)] { answer->set_value(dialog()); });

    while (future.wait_for(std::chrono::milliseconds { 100 }) != std::future_status::ready) {
        if (_closing) return ifClosing;
    }

    try {
        return future.get();
    } catch (const std::future_error&) {
        // dropped
        return ifClosing;
    }
}

//
// Between the threads
//

void NativeShell::_postToUi(std::function<void()> task) {
    bool wake = false;
    {
        const std::scoped_lock lock { _postedMutex };
        if (!_posting) return;
        _posted.push_back(std::move(task));
        wake = _posted.size() == 1;
    }
    if (wake) _wakeUiThread();
}

void NativeShell::_runPosted() {
    assert(_isUiThread());

    std::deque<std::function<void()>> tasks;
    {
        const std::scoped_lock lock { _postedMutex };
        tasks.swap(_posted);
    }
    for (const auto& task : tasks) task();
}

void NativeShell::_startApplication() {
    assert(_isUiThread());

    {
        const std::scoped_lock lock { _postedMutex };
        _posting = true;
    }
    _app = std::make_unique<AppThread>(*this);
}

void NativeShell::_stopApplication() {
    assert(_isUiThread());

    // in that order : a dialog it waits for would never be answered, as this thread is about to wait for it
    _closing = true;
    {
        const std::scoped_lock lock { _postedMutex };
        _posting = false;
        _posted.clear();
    }
    _app.reset();
}

//
// The page
//

void NativeShell::_pageQuery(const std::string& id, const std::string& request) {
    assert(_isUiThread());
    if (!_app) return;

    _app->run([this, viewNumber = _viewNumber, id, request](Controller& controller) {
        _postToUi([this, viewNumber, id, answer = controller.query(request)] {
            if (viewNumber != _viewNumber) return;

            _resolveNow(id, answer);
            _pageLoaded = true;

            if (_pendingPanel) {
                _evalNow("sb.receive(" + page::panelEvent(*_pendingPanel) + ");");
                _pendingPanel.reset();
            }
        });
    });
}

void NativeShell::_pageCommand(const std::string& id, const std::string& request) {
    assert(_isUiThread());

    if (_app) _app->run([request](Controller& controller) { controller.command(request); });
    _resolveNow(id, "null");
}

void NativeShell::_viewCreated() {
    assert(_isUiThread());

    _pageLoaded = false;
    ++_viewNumber;
}

void NativeShell::_viewDestroyed() {
    assert(_isUiThread());

    _pageLoaded = false;
    ++_viewNumber;
}

//
// Menus
//

void NativeShell::_openPanel(Panel panel) {
    if (_pageLoaded) {
        _evalNow("sb.receive(" + page::panelEvent(panel) + ");");
    } else {
        // once the page shown next is loaded
        _pendingPanel = panel;
    }

    _showNow();
}

void NativeShell::_runMenuCommand(MenuCommand command) {
    assert(_isUiThread());

    const auto onApplication = [this](std::function<void(Controller&)> task) {
        if (_app) _app->run(std::move(task));
    };

    switch (command) {
        case MenuCommand::OpenMonitor:
            _showNow();
            break;
        case MenuCommand::Preferences:
            _openPanel(Panel::Preferences);
            break;
        case MenuCommand::MyPlatform:
            onApplication([](Controller& controller) { controller.openPlatform(); });
            break;
        case MenuCommand::Warnings:
            _openPanel(Panel::Warnings);
            break;
        case MenuCommand::DataFolder:
            onApplication([](Controller&) { Controller::openDataFolder(); });
            break;
        case MenuCommand::About:
            _openPanel(Panel::About);
            break;
        case MenuCommand::Quit:
            onApplication([](Controller& controller) { controller.requestQuit(); });
            break;
    }
}

}  // namespace ui

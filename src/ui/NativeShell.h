// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "src/app/AppThread.h"
#include "src/app/Controller.h"
#include "src/app/PageMessages.h"

namespace ui {

// the entries of the tray or menu bar menu
enum class MenuCommand : int { OpenMonitor = 1, Preferences, MyPlatform, Warnings, DataFolder, Quit, About };

// What the native shells share : they only display, on the user interface thread, and exchange messages with the application thread.
// The Shell calls come from that thread, and are handed over. All the rest is on the user interface thread
class NativeShell : public Shell {
 public:
    void sendToPage(const std::string& event) final;
    void setTrayState(TrayState state) final;
    bool ask(std::string_view title, std::string_view question, Answer byDefault) final;
    void tell(std::string_view title, std::string_view text, Severity severity) final;
    void quit(int exitCode) final;

 protected:
    NativeShell() = default;

    // once the shell is ready to be called
    void _startApplication();

    // before the shell goes : what the application is waiting for gives up, then its workers stop. Can be called twice
    void _stopApplication();

    // "task" later on the user interface thread, unless it is closing
    void _postToUi(std::function<void()> task);

    // what _postToUi() was given : to run when _wakeUiThread() asks
    void _runPosted();

    // the webview's calls, answered from the application thread : the page waits for it when it is busy, not the window
    void _pageQuery(const std::string& id, const std::string& request);
    void _pageCommand(const std::string& id, const std::string& request);

    void _viewCreated();
    void _viewDestroyed();

    void _runMenuCommand(MenuCommand command);

    // as last told by the application
    bool _canOpenPlatform() const { return _lastCanOpenPlatform; }

    // what each platform does. Called on the user interface thread, except _wakeUiThread() and _isUiThread()
    virtual void _wakeUiThread() = 0;
    virtual bool _isUiThread() const = 0;
    virtual void _showNow() = 0;
    virtual void _applyTrayState(TrayState state) = 0;
    virtual bool _askNow(const std::string& title, const std::string& question, Answer byDefault) = 0;
    virtual void _tellNow(const std::string& title, const std::string& text, Severity severity) = 0;
    virtual void _quitNow(int exitCode) = 0;
    virtual void _evalNow(const std::string& script) = 0;
    virtual void _resolveNow(const std::string& id, const std::string& answer) = 0;

 private:
    std::mutex _postedMutex;
    std::deque<std::function<void()>> _posted;
    bool _posting = false;

    std::atomic<bool> _closing = false;  // dialogs stop being waited for

    // on the user interface thread only
    std::uint64_t _viewNumber = 0;  // what is answered to a view gone meanwhile is dropped
    bool _pageLoaded = false;
    std::optional<Panel> _pendingPanel;
    bool _lastCanOpenPlatform = false;

    // created once the shell is ready, destroyed first
    std::unique_ptr<AppThread> _app;

    void _openPanel(Panel panel);
    bool _dialogAnswer(bool ifClosing, std::function<bool()> dialog);
};

}  // namespace ui

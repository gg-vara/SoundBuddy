// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <windows.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <string_view>

#include "src/app/Controller.h"
#include "src/ui/NativeShell.h"

namespace ui {

// Native shell on Windows : a window hosting the page in WebView2, and a tray icon.
// The window lives hidden in the tray : its webview exists only while it is shown, to spare memory.
class MainWindow final : public NativeShell {
 public:
    // hidden, except in debug builds
    MainWindow();
    ~MainWindow() override;

    MainWindow(const MainWindow&) = delete;
    void operator=(const MainWindow&) = delete;

    // runs the user interface until quit() : returns the exit code
    static int run();

 protected:
    void _wakeUiThread() override;
    bool _isUiThread() const override;
    void _showNow() override;
    void _applyTrayState(TrayState state) override;
    bool _askNow(const std::string& title, const std::string& question, Answer byDefault) override;
    void _tellNow(const std::string& title, const std::string& text, Severity severity) override;
    void _quitNow(int exitCode) override;
    void _evalNow(const std::string& script) override;
    void _resolveNow(const std::string& id, const std::string& answer) override;

 private:
    HWND _window = nullptr;
    NOTIFYICONDATAW _trayIcon {};
    TrayState _trayState;
    UINT _taskbarCreatedMessage = 0;
    bool _centered = false;

    // the webview, while the window is shown
    struct View;
    std::unique_ptr<View> _view;
    bool _creatingView = false;

    static LRESULT CALLBACK _windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT _handle(UINT message, WPARAM wParam, LPARAM lParam);

    void _hideWindow();
    void _createView();
    void _destroyView();
    void _resizeView();
    void _focusView();
    void _configureView();

    void _addTrayIcon();
    void _updateTrayIcon();
    void _showTrayMenu();
    void _onMenuCommand(UINT command);

    void _applyTheme();
    void _center();

    // owner of the modal dialogs : the window if shown
    HWND _dialogOwner() const;
};

}  // namespace ui

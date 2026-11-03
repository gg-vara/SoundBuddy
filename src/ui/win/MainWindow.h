// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <windows.h>
#include <shellapi.h>

#include <memory>
#include <string>
#include <string_view>

#include "src/ui/Controller.h"

namespace ui {

// Native shell on Windows : a window hosting the page in WebView2, and a tray icon.
// The window lives hidden in the tray : its webview exists only while it is shown, to spare memory.
class MainWindow final : public Shell {
 public:
    // hidden, except in debug builds
    MainWindow();
    ~MainWindow() override;

    MainWindow(const MainWindow&) = delete;
    void operator=(const MainWindow&) = delete;

    // runs the user interface until quit() : returns the exit code
    static int run();

    // Shell
    bool sendToPage(const std::string& event) override;
    void showWindow() override;
    bool ask(std::string_view title, std::string_view question, Answer byDefault) override;
    void tell(std::string_view title, std::string_view text, Severity severity) override;
    void quit() override;

 private:
    HWND _window = nullptr;
    NOTIFYICONDATAW _trayIcon {};
    UINT _taskbarCreatedMessage = 0;
    bool _centered = false;

    // the webview, while the window is shown
    struct View;
    std::unique_ptr<View> _view;
    bool _creatingView = false;
    bool _pageLoaded = false;

    // created once the window exists, destroyed first
    std::unique_ptr<Controller> _controller;

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

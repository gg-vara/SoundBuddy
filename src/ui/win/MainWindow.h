// SoundBuddy
// Small companion app that feeds SoundVitrine with ITunes / Music library metadata
// Copyright (C) 2019-2023 Guillaume Vara <guillaume.vara@gmail.com>

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Any graphical or audio resources available within the source code may
// use a different license and copyright : please refer to their metadata
// for further details. Resources without explicit references to a
// different license and copyright still refer to this GPL.

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
    void _showTrayMenu();
    void _onMenuCommand(UINT command);

    void _applyTheme();
    void _center();

    // owner of the modal dialogs : the window if shown
    HWND _dialogOwner() const;
};

}  // namespace ui

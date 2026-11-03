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

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

// The page's webview on macOS, behind an interface free of webview.h : its Cocoa engine keeps Objective-C objects
// as C++ members, which ARC rejects. WebView.mac.cpp, plain C++ without ARC, compiles it
namespace ui {

class WebView {
 public:
    // fills the content of "window", an NSWindow : throws std::runtime_error if WebKit is unavailable
    WebView(bool devTools, void* window);
    ~WebView();

    WebView(const WebView&) = delete;
    void operator=(const WebView&) = delete;

    // "function" answers the calls of the page to window.<name>(...), with their arguments as a JSON array
    void bind(const std::string& name, std::function<std::string(const std::string&)> function);
    void eval(const std::string& script);
    void setHtml(const std::string& html);

 private:
    struct Engine;
    std::unique_ptr<Engine> _engine;
};

}  // namespace ui

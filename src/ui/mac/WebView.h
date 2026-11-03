// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

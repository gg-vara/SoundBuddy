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

#ifdef __APPLE__

#include "WebView.h"

#include <webview/webview.h>

#include <utility>

namespace ui {

struct WebView::Engine {
    Engine(bool devTools, void* window) :
        browser(devTools, window) {}

    webview::webview browser;
};

WebView::WebView(bool devTools, void* window) {
    try {
        _engine = std::make_unique<Engine>(devTools, window);
    } catch (const webview::exception& e) {
        throw std::runtime_error(e.what());
    }
}

WebView::~WebView() = default;

void WebView::bind(const std::string& name, std::function<std::string(const std::string&)> function) {
    _engine->browser.bind(name, [function = std::move(function)](const std::string& request) { return function(request); });
}

void WebView::eval(const std::string& script) {
    _engine->browser.eval(script);
}

void WebView::setHtml(const std::string& html) {
    _engine->browser.set_html(html);
}

}  // namespace ui

#endif

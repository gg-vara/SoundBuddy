// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

void WebView::bind(const std::string& name, std::function<void(const std::string& id, const std::string& request)> function) {
    _engine->browser.bind(
        name,
        [function = std::move(function)](const std::string& id, const std::string& request, void* /*argument*/) { function(id, request); },
        nullptr
    );
}

void WebView::resolve(const std::string& id, const std::string& answer) {
    _engine->browser.resolve(id, 0, answer);
}

void WebView::eval(const std::string& script) {
    _engine->browser.eval(script);
}

void WebView::setHtml(const std::string& html) {
    _engine->browser.set_html(html);
}

}  // namespace ui

#endif

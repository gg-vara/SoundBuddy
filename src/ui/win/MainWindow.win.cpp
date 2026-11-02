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

#ifdef _WIN32

#include "MainWindow.h"

#include <webview/webview.h>

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#include "page.h"
#include "src/helpers/Platform.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/ui/Controller.h"
#include "src/ui/Post.h"
#include "src/ui/Run.h"
#include "version.h"

namespace ui {

namespace {

// posted to the window
constexpr UINT postedTasksMessage = WM_APP + 1;
constexpr UINT trayIconMessage = WM_APP + 2;

// tray menu
enum class MenuCommand : UINT { OpenMonitor = 1, Preferences, MyPlatform, Warnings, DataFolder, Quit };

// size of the window, in pixels at 96 DPI
constexpr int defaultWidth = 760;
constexpr int defaultHeight = 600;
constexpr int minimumWidth = 480;
constexpr int minimumHeight = 400;

// background of the page (--bg in src/ui/page/style.css), shown until it is painted
constexpr COLORREF lightBackground = RGB(0xf4, 0xf2, 0xee);
constexpr COLORREF darkBackground = RGB(0x16, 0x15, 0x14);

const wchar_t* const windowClass = L"SoundBuddyWindow";

#ifdef _DEBUG
const char* const windowTitle = "DEBUG - " APP_NAME;
constexpr bool devTools = true;
#else
const char* const windowTitle = APP_NAME;
constexpr bool devTools = false;
#endif

// tasks posted by the workers, run by the window
struct PostedTasks {
    std::mutex mutex;
    HWND window = nullptr;  // none : tasks are dropped
    std::deque<std::function<void()>> tasks;
};

PostedTasks& postedTasks() {
    static PostedTasks instance;
    return instance;
}

void setPostTarget(HWND window) {
    auto& posted = postedTasks();
    const std::scoped_lock lock { posted.mutex };
    posted.window = window;
    if (!window) posted.tasks.clear();
}

void runPostedTasks() {
    std::deque<std::function<void()>> tasks;
    {
        auto& posted = postedTasks();
        const std::scoped_lock lock { posted.mutex };
        tasks.swap(posted.tasks);
    }
    for (auto& task : tasks) task();
}

// "setting" of the personalization : AppsUseLightTheme for windows, SystemUsesLightTheme for the taskbar
bool isDark(const wchar_t* setting) {
    DWORD lightTheme = 1;
    DWORD size = sizeof(lightTheme);
    RegGetValueW(
        HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", setting, RRF_RT_REG_DWORD, nullptr,
        &lightTheme, &size
    );
    return lightTheme == 0;
}

bool isDarkTheme() {
    return isDark(L"AppsUseLightTheme");
}

int scaled(int pixels, HWND window) {
    return MulDiv(pixels, static_cast<int>(GetDpiForWindow(window)), USER_DEFAULT_SCREEN_DPI);
}

// icon resource "name", in the size the system uses at its scaling : the closest image of the icon, scaled down if needed
HICON loadIcon(const wchar_t* name, int metric) {
    HICON icon = nullptr;
    LoadIconMetric(GetModuleHandleW(nullptr), name, metric, &icon);
    return icon;
}

// on the taskbar's theme
const wchar_t* trayIconName() {
    return isDark(L"SystemUsesLightTheme") ? L"TRAY_DARK" : L"TRAY_LIGHT";
}

// pointer carried by a message parameter, or a window data
template <typename T, typename Integer>
T* pointerOf(Integer value) {
    return reinterpret_cast<T*>(value);  // NOLINT(performance-no-int-to-ptr) : Win32 passes pointers as integers
}

}  // namespace

struct MainWindow::View {
    View(bool debug, HWND window) :
        browser(debug, window) {}

    webview::webview browser;
};

void post(std::function<void()> task) {
    auto& posted = postedTasks();
    const std::scoped_lock lock { posted.mutex };
    if (!posted.window) return;

    posted.tasks.push_back(std::move(task));
    if (posted.tasks.size() == 1) PostMessageW(posted.window, postedTasksMessage, 0, 0);
}

MainWindow::MainWindow() {
    auto* const instance = GetModuleHandleW(nullptr);

    const WNDCLASSEXW windowClassInfos {
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = &MainWindow::_windowProc,
        .hInstance = instance,
        .hIcon = loadIcon(L"APP", LIM_LARGE),
        .hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)),  // IDC_ARROW
        .lpszClassName = windowClass,
        .hIconSm = loadIcon(L"APP", LIM_SMALL),
    };
    RegisterClassExW(&windowClassInfos);

    CreateWindowExW(
        0, windowClass, platform::widen(windowTitle).c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, nullptr, nullptr, instance, this
    );
    if (!_window) throw std::runtime_error("Could not create the main window");

    SetWindowPos(
        _window, nullptr, 0, 0, scaled(defaultWidth, _window), scaled(defaultHeight, _window), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
    );
    _applyTheme();

    _taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    _addTrayIcon();

    setPostTarget(_window);
    _controller = std::make_unique<Controller>(*this);

#ifdef _DEBUG
    showWindow();
#endif
}

MainWindow::~MainWindow() {
    // workers stop first, while their last tasks are dropped
    _controller.reset();
    setPostTarget(nullptr);

    _destroyView();
    Shell_NotifyIconW(NIM_DELETE, &_trayIcon);
    if (_trayIcon.hIcon) DestroyIcon(_trayIcon.hIcon);
    DestroyWindow(_window);
}

int MainWindow::run() {
    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

//
// Shell
//

bool MainWindow::sendToPage(const std::string& event) {
    if (!_view || !_pageLoaded) return false;
    _view->browser.eval("sb.receive(" + event + ");");
    return true;
}

void MainWindow::showWindow() {
    if (!_centered) {
        _center();
        _centered = true;
    }

    ShowWindow(_window, IsIconic(_window) != FALSE ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(_window);

    if (!_view && !_creatingView) _createView();
    _focusView();
}

bool MainWindow::ask(std::string_view title, std::string_view question, Answer byDefault) {
    const UINT defaultButton = byDefault == Answer::No ? MB_DEFBUTTON2 : MB_DEFBUTTON1;
    const auto answer = MessageBoxW(
        _dialogOwner(), platform::widen(question).c_str(), platform::widen(title).c_str(),
        MB_YESNO | MB_ICONWARNING | MB_SETFOREGROUND | defaultButton
    );
    return answer == IDYES;
}

void MainWindow::tell(std::string_view title, std::string_view text, Severity severity) {
    const UINT icon = severity == Severity::Error ? MB_ICONERROR : MB_ICONINFORMATION;
    MessageBoxW(_dialogOwner(), platform::widen(text).c_str(), platform::widen(title).c_str(), MB_OK | MB_SETFOREGROUND | icon);
}

void MainWindow::quit() {
    // the webview pumps messages while it is created : it ends first
    if (_creatingView) {
        post([this] { quit(); });
        return;
    }

    ShowWindow(_window, SW_HIDE);
    _destroyView();
    Shell_NotifyIconW(NIM_DELETE, &_trayIcon);
    PostQuitMessage(0);
}

HWND MainWindow::_dialogOwner() const {
    return IsWindowVisible(_window) != FALSE ? _window : nullptr;
}

//
// Window
//

LRESULT CALLBACK MainWindow::_windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* const self = static_cast<MainWindow*>(pointerOf<CREATESTRUCTW>(lParam)->lpCreateParams);
        self->_window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* const self = pointerOf<MainWindow>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (!self) return DefWindowProcW(window, message, wParam, lParam);
    return self->_handle(message, wParam, lParam);
}

LRESULT MainWindow::_handle(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == _taskbarCreatedMessage) {
        // Explorer restarted : the tray icon is gone
        _addTrayIcon();
        return 0;
    }

    switch (message) {
        case postedTasksMessage:
            runPostedTasks();
            return 0;

        case trayIconMessage:
            switch (LOWORD(lParam)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    showWindow();
                    break;
                case WM_CONTEXTMENU:
                    _showTrayMenu();
                    break;
                default:
                    break;
            }
            return 0;

        // the window hides in the tray
        case WM_CLOSE:
            _hideWindow();
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                _hideWindow();
                return 0;
            }
            break;

        case WM_SIZE:
            _resizeView();
            return 0;

        case WM_SETFOCUS:
            _focusView();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* const limits = pointerOf<MINMAXINFO>(lParam);
            limits->ptMinTrackSize = { .x = scaled(minimumWidth, _window), .y = scaled(minimumHeight, _window) };
            return 0;
        }

        case WM_DPICHANGED: {
            const auto* const suggested = pointerOf<const RECT>(lParam);
            SetWindowPos(
                _window, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE
            );
            return 0;
        }

        case WM_SETTINGCHANGE:
            if (lParam != 0 && lstrcmpW(pointerOf<const wchar_t>(lParam), L"ImmersiveColorSet") == 0) {
                _applyTheme();
                _updateTrayIcon();
            }
            break;

        case WM_ERASEBKGND: {
            RECT area {};
            GetClientRect(_window, &area);
            auto* const brush = CreateSolidBrush(isDarkTheme() ? darkBackground : lightBackground);
            FillRect(pointerOf<HDC__>(wParam), &area, brush);
            DeleteObject(brush);
            return 1;
        }

        // the session ends : the process may be terminated once this returns
        case WM_ENDSESSION:
            if (wParam != FALSE) {
                _controller.reset();
                setPostTarget(nullptr);
                Shell_NotifyIconW(NIM_DELETE, &_trayIcon);
            }
            return 0;

        default:
            break;
    }

    return DefWindowProcW(_window, message, wParam, lParam);
}

void MainWindow::_hideWindow() {
    ShowWindow(_window, SW_HIDE);

    // the page and its browser processes go away until shown again
    _destroyView();
}

void MainWindow::_center() {
    RECT frame {};
    GetWindowRect(_window, &frame);

    MONITORINFO monitor { .cbSize = sizeof(MONITORINFO) };
    GetMonitorInfoW(MonitorFromWindow(_window, MONITOR_DEFAULTTOPRIMARY), &monitor);
    const auto& area = monitor.rcWork;

    const auto width = frame.right - frame.left;
    const auto height = frame.bottom - frame.top;
    SetWindowPos(
        _window, nullptr, area.left + ((area.right - area.left - width) / 2), area.top + ((area.bottom - area.top - height) / 2), 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
    );
}

void MainWindow::_applyTheme() {
    const BOOL dark = isDarkTheme() ? TRUE : FALSE;
    DwmSetWindowAttribute(_window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    InvalidateRect(_window, nullptr, TRUE);
}

//
// Webview
//

void MainWindow::_createView() {
    _creatingView = true;
    _pageLoaded = false;

    try {
        // pumps messages until WebView2 is ready
        _view = std::make_unique<View>(devTools, _window);
    } catch (const webview::exception& e) {
        _creatingView = false;
        logs::error("UI", "Webview unavailable : {}", e.what());
        ShowWindow(_window, SW_HIDE);
        tell(
            APP_NAME,
            i18n::format(
                i18n::tr("%1 needs the Microsoft Edge WebView2 Runtime to show its window. Please install it, then try again."),
                { APP_NAME }
            ),
            Severity::Error
        );
        return;
    }

    _creatingView = false;

    // hidden meanwhile
    if (IsWindowVisible(_window) == FALSE) {
        _destroyView();
        return;
    }

    _configureView();
    _resizeView();

    // page queries are answered right away; commands may wait for workers or open dialogs, out of WebView2's callbacks
    _view->browser.bind("sbQuery", [this](const std::string& request) -> std::string {
        const auto answer = _controller->query(request);
        _pageLoaded = true;
        return answer;
    });
    _view->browser.bind("sbCommand", [this](const std::string& request) -> std::string {
        post([this, request] {
            if (_controller) _controller->command(request);
        });
        return "null";
    });

    _view->browser.set_html(page::html);
    _focusView();
}

void MainWindow::_destroyView() {
    _pageLoaded = false;
    _view.reset();
}

void MainWindow::_resizeView() {
    if (!_view) return;

    const auto widget = _view->browser.widget();
    if (!widget.ok()) return;

    RECT area {};
    GetClientRect(_window, &area);
    MoveWindow(static_cast<HWND>(widget.value()), 0, 0, area.right - area.left, area.bottom - area.top, TRUE);
}

void MainWindow::_focusView() {
    if (!_view) return;

    const auto controller = _view->browser.browser_controller();
    if (!controller.ok()) return;
    static_cast<ICoreWebView2Controller*>(controller.value())->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

// the page behaves as an application, rather than as a browser
void MainWindow::_configureView() {
    const auto browser = _view->browser.browser_controller();
    if (!browser.ok()) return;
    auto* const controller = static_cast<ICoreWebView2Controller*>(browser.value());

    // no white flash before the page is painted
    ICoreWebView2Controller2* controller2 = nullptr;
    if (SUCCEEDED(controller->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void**>(&controller2)))) {
        const auto background = isDarkTheme() ? darkBackground : lightBackground;
        controller2->put_DefaultBackgroundColor(
            { .A = 255, .R = GetRValue(background), .G = GetGValue(background), .B = GetBValue(background) }
        );
        controller2->Release();
    }

    if (devTools) return;

    ICoreWebView2* webView = nullptr;
    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(controller->get_CoreWebView2(&webView)) && SUCCEEDED(webView->get_Settings(&settings))) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsZoomControlEnabled(FALSE);

        // reload, find, print...
        ICoreWebView2Settings3* settings3 = nullptr;
        if (SUCCEEDED(settings->QueryInterface(IID_ICoreWebView2Settings3, reinterpret_cast<void**>(&settings3)))) {
            settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
            settings3->Release();
        }
    }
    if (settings) settings->Release();
    if (webView) webView->Release();
}

//
// Tray
//

void MainWindow::_addTrayIcon() {
    if (!_trayIcon.hIcon) {
        _trayIcon = {
            .cbSize = sizeof(NOTIFYICONDATAW),
            .hWnd = _window,
            .uID = 1,
            .uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP,
            .uCallbackMessage = trayIconMessage,
            .hIcon = loadIcon(trayIconName(), LIM_SMALL),
        };
        const auto tip = platform::widen(windowTitle);
        std::copy_n(tip.c_str(), std::min(tip.size() + 1, std::size(_trayIcon.szTip) - 1), std::begin(_trayIcon.szTip));
        _trayIcon.uVersion = NOTIFYICON_VERSION_4;
    }

    Shell_NotifyIconW(NIM_ADD, &_trayIcon);
    Shell_NotifyIconW(NIM_SETVERSION, &_trayIcon);
}

// the taskbar's theme changed
void MainWindow::_updateTrayIcon() {
    auto* const previous = _trayIcon.hIcon;
    _trayIcon.hIcon = loadIcon(trayIconName(), LIM_SMALL);
    Shell_NotifyIconW(NIM_MODIFY, &_trayIcon);
    if (previous) DestroyIcon(previous);
}

void MainWindow::_showTrayMenu() {
    auto* const menu = CreatePopupMenu();
    const auto add = [menu](MenuCommand command, std::string_view text, bool enabled = true) {
        AppendMenuW(menu, MF_STRING | (enabled ? MF_ENABLED : MF_GRAYED), static_cast<UINT_PTR>(command), platform::widen(text).c_str());
    };

    add(MenuCommand::OpenMonitor, i18n::tr("Open monitor..."));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(MenuCommand::Preferences, i18n::tr("Preferences"));
    add(MenuCommand::MyPlatform, i18n::format(i18n::tr("My %1"), { DEST_PLATFORM_PRODUCT_NAME }), _controller->canOpenPlatform());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(MenuCommand::Warnings, i18n::tr("Read latest upload warnings report"), Controller::hasWarnings());
    add(MenuCommand::DataFolder, i18n::tr("Access upload data folder"));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    add(MenuCommand::Quit, i18n::tr("Quit"));
    SetMenuDefaultItem(menu, static_cast<UINT>(MenuCommand::OpenMonitor), FALSE);

    // the menu closes when clicking elsewhere only if the window is in the foreground
    POINT cursor {};
    GetCursorPos(&cursor);
    SetForegroundWindow(_window);
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, _window, nullptr);
    PostMessageW(_window, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (command != 0) _onMenuCommand(static_cast<UINT>(command));
}

void MainWindow::_onMenuCommand(UINT command) {
    switch (static_cast<MenuCommand>(command)) {
        case MenuCommand::OpenMonitor:
            showWindow();
            break;
        case MenuCommand::Preferences:
            _controller->openPanel(Panel::Preferences);
            break;
        case MenuCommand::MyPlatform:
            _controller->openPlatform();
            break;
        case MenuCommand::Warnings:
            _controller->openPanel(Panel::Warnings);
            break;
        case MenuCommand::DataFolder:
            Controller::openDataFolder();
            break;
        case MenuCommand::Quit:
            _controller->requestQuit();
            break;
        default:
            break;
    }
}

//
// Entry point
//

int run(int /*argc*/, char** /*argv*/) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // WebView2 runs in this single-threaded apartment
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // instead of next to the executable, which updates replace
    SetEnvironmentVariableW(L"WEBVIEW2_USER_DATA_FOLDER", (platform::dataDirectory() / "webview").c_str());

    int exitCode = 0;
    {
        const MainWindow window;
        exitCode = MainWindow::run();
    }

    CoUninitialize();
    return exitCode;
}

}  // namespace ui

#endif

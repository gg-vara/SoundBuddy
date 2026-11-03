// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Native shell on macOS : a window hosting the page in WKWebView, and a menu bar item, in an agent application (LSUIElement).
// As on Windows (src/ui/win), the window lives hidden : its webview exists only while it is shown, to spare memory.

#import <AppKit/AppKit.h>

#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "page.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/ui/Controller.h"
#include "src/ui/Post.h"
#include "src/ui/Run.h"
#include "src/ui/mac/WebView.h"
#include "version.h"

namespace ui {
class MacShell;
}

// forwards the window and menus' events to the shell
@interface SBShellTarget : NSObject <NSWindowDelegate, NSMenuDelegate>
@property(nonatomic, assign) ui::MacShell* shell;
- (void)menuAction:(NSMenuItem*)item;
@end

namespace ui {

namespace {

// menu bar item's menu
enum class MenuCommand : NSInteger { OpenMonitor = 1, Preferences, MyPlatform, Warnings, DataFolder, Quit };

// size of the window, in points
constexpr CGFloat defaultWidth = 760;
constexpr CGFloat defaultHeight = 600;
constexpr CGFloat minimumWidth = 480;
constexpr CGFloat minimumHeight = 400;

#ifdef _DEBUG
const char* const windowTitle = "DEBUG - " APP_NAME;
constexpr bool devTools = true;
#else
const char* const windowTitle = APP_NAME;
constexpr bool devTools = false;
#endif

// tasks posted by the workers run on the main queue, while the user interface accepts them
std::atomic<bool>& acceptingTasks() {
    static std::atomic<bool> accepting { false };
    return accepting;
}

NSString* toNSString(std::string_view text) {
    return [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];
}

// an agent application comes to the front for its windows and dialogs
void activate() {
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    } else {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

}  // namespace

void post(std::function<void()> task) {
    if (!acceptingTasks()) return;

    auto* const posted =
        new std::function<void()>(std::move(task));  // NOLINT(cppcoreguidelines-owning-memory) : owned by the queue until run
    dispatch_async_f(dispatch_get_main_queue(), posted, [](void* context) {
        const std::unique_ptr<std::function<void()>> task { static_cast<std::function<void()>*>(context) };
        if (acceptingTasks()) (*task)();
    });
}

class MacShell final : public Shell {
 public:
    // hidden, except in debug builds
    MacShell() {
        _target = [[SBShellTarget alloc] init];
        _target.shell = this;

        _createMainMenu();
        _createWindow();
        _createStatusItem();

        acceptingTasks() = true;
        _controller = std::make_unique<Controller>(*this);

#ifdef _DEBUG
        showWindow();
#endif
    }

    ~MacShell() override {
        // workers stop first, while their last tasks are dropped
        _controller.reset();
        acceptingTasks() = false;

        _destroyView();
        if (_statusItem) [NSStatusBar.systemStatusBar removeStatusItem:_statusItem];
        _window.delegate = nil;
        [_window close];
    }

    MacShell(const MacShell&) = delete;
    void operator=(const MacShell&) = delete;

    // runs the user interface until quit()
    static int run() {
        [NSApp run];
        return 0;
    }

    // Shell

    bool sendToPage(const std::string& event) override {
        if (!_view || !_pageLoaded) return false;
        _view->eval("sb.receive(" + event + ");");
        return true;
    }

    void showWindow() override {
        activate();
        if (!_view) _createView();
        [_window makeKeyAndOrderFront:nil];
    }

    bool ask(std::string_view title, std::string_view question, Answer byDefault) override {
        NSAlert* const alert = [[NSAlert alloc] init];
        alert.alertStyle = NSAlertStyleWarning;
        alert.messageText = toNSString(title);
        alert.informativeText = toNSString(question);
        NSButton* const yes = [alert addButtonWithTitle:toNSString(i18n::tr("Yes"))];
        NSButton* const no = [alert addButtonWithTitle:toNSString(i18n::tr("No"))];
        if (byDefault == Answer::No) {
            yes.keyEquivalent = @"";
            no.keyEquivalent = @"\r";
        }

        activate();
        return [alert runModal] == NSAlertFirstButtonReturn;
    }

    void tell(std::string_view title, std::string_view text, Severity severity) override {
        NSAlert* const alert = [[NSAlert alloc] init];
        alert.alertStyle = severity == Severity::Error ? NSAlertStyleCritical : NSAlertStyleInformational;
        alert.messageText = toNSString(title);
        alert.informativeText = toNSString(text);
        [alert addButtonWithTitle:toNSString(i18n::tr("OK"))];

        activate();
        [alert runModal];
    }

    void quit() override {
        [_window orderOut:nil];
        _destroyView();

        // the run loop ends once it handles an event
        [NSApp stop:nil];
        [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSZeroPoint
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0]
                 atStart:YES];
    }

    // SBShellTarget

    // the window hides in the menu bar
    void onWindowClosing() {
        [_window orderOut:nil];

        // the page and its web content process go away until shown again
        _destroyView();
    }

    void onMenuOpening() {
        [_statusMenu itemWithTag:static_cast<NSInteger>(MenuCommand::MyPlatform)].enabled = _controller->canOpenPlatform();
        [_statusMenu itemWithTag:static_cast<NSInteger>(MenuCommand::Warnings)].enabled = Controller::hasWarnings();
    }

    void onMenuCommand(NSInteger command) {
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
        }
    }

 private:
    SBShellTarget* _target = nil;
    NSWindow* _window = nil;
    NSStatusItem* _statusItem = nil;
    NSMenu* _statusMenu = nil;

    // the webview, while the window is shown
    std::unique_ptr<WebView> _view;
    bool _pageLoaded = false;

    // created once the window exists, destroyed first
    std::unique_ptr<Controller> _controller;

    // an agent application shows no menu bar, but its key equivalents work : quit, and editing the page's fields
    void _createMainMenu() {
        NSMenu* const mainMenu = [[NSMenu alloc] init];

        NSMenuItem* const appItem = [mainMenu addItemWithTitle:@"" action:nil keyEquivalent:@""];
        NSMenu* const appMenu = [[NSMenu alloc] init];
        NSMenuItem* const quit = [appMenu addItemWithTitle:toNSString(i18n::tr("Quit")) action:@selector(menuAction:) keyEquivalent:@"q"];
        quit.tag = static_cast<NSInteger>(MenuCommand::Quit);
        quit.target = _target;
        appItem.submenu = appMenu;

        // standard actions, sent to the focused field
        NSMenuItem* const editItem = [mainMenu addItemWithTitle:@"Edit" action:nil keyEquivalent:@""];
        NSMenu* const editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
        [editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
        [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
        [editMenu addItem:NSMenuItem.separatorItem];
        [editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
        [editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
        [editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
        [editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
        editItem.submenu = editMenu;

        NSApp.mainMenu = mainMenu;
    }

    void _createWindow() {
        _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, defaultWidth, defaultHeight)
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
        _window.releasedWhenClosed = NO;
        _window.title = toNSString(windowTitle);
        _window.contentMinSize = NSMakeSize(minimumWidth, minimumHeight);
        _window.delegate = _target;
        [_window center];
    }

    void _createStatusItem() {
        _statusItem = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];

        // a template image follows the menu bar's appearance
        NSImage* const image = [NSImage imageNamed:@"app_reverse"];
        image.size = NSMakeSize(18, 18);
        [image setTemplate:YES];  // "template" is a C++ keyword
        _statusItem.button.image = image;
        _statusItem.button.toolTip = toNSString(windowTitle);

        _statusMenu = [[NSMenu alloc] init];
        _statusMenu.autoenablesItems = NO;
        _statusMenu.delegate = _target;

        const auto add = [this](MenuCommand command, std::string_view text) {
            NSMenuItem* const item = [_statusMenu addItemWithTitle:toNSString(text) action:@selector(menuAction:) keyEquivalent:@""];
            item.tag = static_cast<NSInteger>(command);
            item.target = _target;
        };
        add(MenuCommand::OpenMonitor, i18n::tr("Open monitor..."));
        [_statusMenu addItem:NSMenuItem.separatorItem];
        add(MenuCommand::Preferences, i18n::tr("Preferences"));
        add(MenuCommand::MyPlatform, i18n::format(i18n::tr("My %1"), { DEST_PLATFORM_PRODUCT_NAME }));
        [_statusMenu addItem:NSMenuItem.separatorItem];
        add(MenuCommand::Warnings, i18n::tr("Read latest upload warnings report"));
        add(MenuCommand::DataFolder, i18n::tr("Access upload data folder"));
        [_statusMenu addItem:NSMenuItem.separatorItem];
        add(MenuCommand::Quit, i18n::tr("Quit"));

        _statusItem.menu = _statusMenu;
    }

    void _createView() {
        _pageLoaded = false;

        try {
            // fills the window's content
            _view = std::make_unique<WebView>(devTools, (__bridge void*)_window);
        } catch (const std::runtime_error& e) {
            logs::error("UI", "Webview unavailable : {}", e.what());
            tell(APP_NAME, e.what(), Severity::Error);
            return;
        }

        // page queries are answered right away; commands may wait for workers or open dialogs, out of WebKit's callbacks
        _view->bind("sbQuery", [this](const std::string& request) -> std::string {
            auto answer = _controller->query(request);
            _pageLoaded = true;
            return answer;
        });
        _view->bind("sbCommand", [this](const std::string& request) -> std::string {
            post([this, request] {
                if (_controller) _controller->command(request);
            });
            return "null";
        });

        _view->setHtml(page::html);
    }

    void _destroyView() {
        _pageLoaded = false;
        _view.reset();
    }
};

int run(int /*argc*/, char** /*argv*/) {
    @autoreleasepool {
        [NSApplication sharedApplication];

        // in the menu bar only, as the bundle declares (LSUIElement)
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        int exitCode = 0;
        {
            MacShell shell;
            exitCode = MacShell::run();
        }
        return exitCode;
    }
}

}  // namespace ui

@implementation SBShellTarget

- (void)menuAction:(NSMenuItem*)item {
    self.shell->onMenuCommand(item.tag);
}

- (void)menuWillOpen:(NSMenu*)menu {
    self.shell->onMenuOpening();
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    self.shell->onWindowClosing();
    return NO;
}

@end

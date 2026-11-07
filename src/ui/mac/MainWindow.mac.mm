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
#include "src/app/Controller.h"
#include "src/app/PageMessages.h"
#include "src/app/AppThread.h"
#include "src/ui/Dialog.h"
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
enum class MenuCommand : NSInteger { OpenMonitor = 1, Preferences, MyPlatform, Warnings, DataFolder, Quit, About };

// size of the window, in points
constexpr CGFloat defaultWidth = 760;
constexpr CGFloat defaultHeight = 600;
constexpr CGFloat minimumWidth = 480;
constexpr CGFloat minimumHeight = 400;

#ifdef _DEBUG
constexpr bool devTools = true;
#else
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

void postToMainQueue(std::function<void()> task) {
    if (!acceptingTasks()) return;

    auto* const posted =
        new std::function<void()>(std::move(task));  // NOLINT(cppcoreguidelines-owning-memory) : owned by the queue until run
    dispatch_async_f(dispatch_get_main_queue(), posted, [](void* context) {
        const std::unique_ptr<std::function<void()>> task { static_cast<std::function<void()>*>(context) };
        if (acceptingTasks()) (*task)();
    });
}

namespace {

// the menu bar icon. Alone, it is a template image, which follows the menu bar's appearance. With colors, a template image
// would lose them : the note is then drawn in the color of the menu bar's labels, which follows it as well
NSImage* menuBarImage(TrayState state) {
    constexpr CGFloat side = 18;
    NSImage* const base = [[NSImage imageNamed:state.shouting ? @"app_reverse_active" : @"app_reverse"] copy];
    base.size = NSMakeSize(side, side);

    if (state.dot == TrayState::Dot::None) {
        [base setTemplate:!state.shouting];  // "template" is a C++ keyword
        return base;
    }

    NSColor* const dotColor = state.dot == TrayState::Dot::Ok
                                  ? [NSColor colorWithSRGBRed:0x2f / 255.0 green:0x9e / 255.0 blue:0x62 / 255.0 alpha:1]
                                  : [NSColor colorWithSRGBRed:0xd2 / 255.0 green:0x3f / 255.0 blue:0x3f / 255.0 alpha:1];
    const bool shouting = state.shouting;

    return [NSImage imageWithSize:NSMakeSize(side, side)
                          flipped:NO
                   drawingHandler:^BOOL(NSRect rect) {
                     [base drawInRect:rect];
                     if (!shouting) {
                         [NSColor.labelColor set];
                         NSRectFillUsingOperation(rect, NSCompositingOperationSourceIn);
                     }

                     // a gap around the dot, in the lower right corner, keeps it apart from the note
                     const NSRect gap = NSMakeRect(side * 0.66, 0, side * 0.34, side * 0.34);
                     [NSColor.clearColor set];
                     NSBezierPath* const gapPath = [NSBezierPath bezierPathWithOvalInRect:gap];
                     NSGraphicsContext.currentContext.compositingOperation = NSCompositingOperationCopy;
                     [gapPath fill];
                     NSGraphicsContext.currentContext.compositingOperation = NSCompositingOperationSourceOver;

                     [dotColor set];
                     [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(gap, side * 0.045, side * 0.045)] fill];
                     return YES;
                   }];
}

}  // namespace

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
        _app = std::make_unique<AppThread>(*this);

#ifdef _DEBUG
        _show();
#endif
    }

    ~MacShell() override {
        // in that order : a dialog it waits for would never be answered, as this thread is about to wait for it
        _closing = true;
        acceptingTasks() = false;
        _app.reset();

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
        if (!_pageLoaded) return false;

        postToMainQueue([this, event] {
            if (_view && _pageLoaded) _view->eval("sb.receive(" + event + ");");
        });
        return true;
    }

    void setTrayState(TrayState state) override {
        postToMainQueue([this, state] {
            _canOpenPlatform = state.canOpenPlatform;
            _statusItem.button.image = menuBarImage(state);
        });
    }

    void showWindow() override {
        postToMainQueue([this] { _show(); });
    }

    bool ask(std::string_view title, std::string_view question, Answer byDefault) override {
        return dialogAnswer(
            &postToMainQueue, _closing, byDefault == Answer::Yes,
            [this, title = std::string { title }, question = std::string { question }, byDefault] {
                return _ask(title, question, byDefault);
            }
        );
    }

    void tell(std::string_view title, std::string_view text, Severity severity) override {
        dialogAnswer(
            &postToMainQueue, _closing, /*ifClosing=*/true, [this, title = std::string { title }, text = std::string { text }, severity] {
                _tell(title, text, severity);
                return true;
            }
        );
    }

    void quit() override {
        postToMainQueue([this] { _quit(); });
    }

    // SBShellTarget

    // the window hides in the menu bar
    void onWindowClosing() {
        [_window orderOut:nil];

        // the page and its web content process go away until shown again
        _destroyView();
    }

    void onMenuOpening() {
        [_statusMenu itemWithTag:static_cast<NSInteger>(MenuCommand::MyPlatform)].enabled = _canOpenPlatform;
        [_statusMenu itemWithTag:static_cast<NSInteger>(MenuCommand::Warnings)].enabled = Controller::hasWarnings();
    }

    void onMenuCommand(NSInteger command) {
        const auto onApplication = [this](auto task) {
            if (_app) _app->run(std::move(task));
        };

        switch (static_cast<MenuCommand>(command)) {
            case MenuCommand::OpenMonitor:
                _show();
                break;
            case MenuCommand::Preferences:
                onApplication([](Controller& controller) { controller.openPanel(Panel::Preferences); });
                break;
            case MenuCommand::MyPlatform:
                onApplication([](Controller& controller) { controller.openPlatform(); });
                break;
            case MenuCommand::Warnings:
                onApplication([](Controller& controller) { controller.openPanel(Panel::Warnings); });
                break;
            case MenuCommand::DataFolder:
                onApplication([](Controller&) { Controller::openDataFolder(); });
                break;
            case MenuCommand::About:
                onApplication([](Controller& controller) { controller.openPanel(Panel::About); });
                break;
            case MenuCommand::Quit:
                onApplication([](Controller& controller) { controller.requestQuit(); });
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
    std::atomic<bool> _pageLoaded = false;       // set from the application thread too
    std::atomic<std::uint64_t> _viewNumber = 0;  // what is answered to a view gone meanwhile is dropped
    bool _canOpenPlatform = false;
    std::atomic<bool> _closing = false;  // dialogs stop being waited for

    // created once the window exists, destroyed first
    std::unique_ptr<AppThread> _app;

    void _show() {
        activate();
        if (!_view) _createView();
        [_window makeKeyAndOrderFront:nil];
    }

    bool _ask(const std::string& title, const std::string& question, Answer byDefault) {
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

    void _tell(const std::string& title, const std::string& text, Severity severity) {
        NSAlert* const alert = [[NSAlert alloc] init];
        alert.alertStyle = severity == Severity::Error ? NSAlertStyleCritical : NSAlertStyleInformational;
        alert.messageText = toNSString(title);
        alert.informativeText = toNSString(text);
        [alert addButtonWithTitle:toNSString(i18n::tr("OK"))];

        activate();
        [alert runModal];
    }

    void _quit() {
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
        _window.title = toNSString(Controller::versionLabel());
        _window.contentMinSize = NSMakeSize(minimumWidth, minimumHeight);
        _window.delegate = _target;
        [_window center];
    }

    void _createStatusItem() {
        _statusItem = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];

        _statusItem.button.image = menuBarImage({});
        _statusItem.button.toolTip = toNSString(Controller::versionLabel());

        _statusMenu = [[NSMenu alloc] init];
        _statusMenu.autoenablesItems = NO;
        _statusMenu.delegate = _target;

        const auto add = [this](MenuCommand command, std::string_view text) {
            NSMenuItem* const item = [_statusMenu addItemWithTitle:toNSString(text) action:@selector(menuAction:) keyEquivalent:@""];
            item.tag = static_cast<NSInteger>(command);
            item.target = _target;
        };
        add(MenuCommand::About, Controller::versionLabel());
        [_statusMenu addItem:NSMenuItem.separatorItem];
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
            _tell(APP_NAME, e.what(), Severity::Error);
            return;
        }

        // answered from the application thread, which may be busy : the page waits for that, not the window
        _view->bind("sbQuery", [this, viewNumber = _viewNumber.load()](const std::string& id, const std::string& request) {
            if (!_app) return;

            _app->run([this, viewNumber, id, request](Controller& controller) {
                auto answer = controller.query(request);

                // what the application sends from now on comes after this answer, which holds what it sent so far
                if (viewNumber == _viewNumber) _pageLoaded = true;

                postToMainQueue([this, viewNumber, id, answer = std::move(answer)] {
                    if (_view && viewNumber == _viewNumber) _view->resolve(id, answer);
                });
            });
        });
        _view->bind("sbCommand", [this](const std::string& id, const std::string& request) {
            if (_app) _app->run([request](Controller& controller) { controller.command(request); });
            _view->resolve(id, "null");
        });

        _view->setHtml(page::html);
    }

    void _destroyView() {
        _pageLoaded = false;
        ++_viewNumber;
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

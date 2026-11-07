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
#include "src/ui/NativeShell.h"
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

class MacShell final : public NativeShell {
 public:
    // hidden, except in debug builds
    MacShell() {
        _target = [[SBShellTarget alloc] init];
        _target.shell = this;

        _createMainMenu();
        _createWindow();
        _createStatusItem();

        _startApplication();

#ifdef _DEBUG
        _showNow();
#endif
    }

    ~MacShell() override {
        _stopApplication();

        _destroyView();
        if (_statusItem) [NSStatusBar.systemStatusBar removeStatusItem:_statusItem];
        _window.delegate = nil;
        [_window close];
    }

    MacShell(const MacShell&) = delete;
    void operator=(const MacShell&) = delete;

    // runs the user interface until quit() : returns the exit code
    int run() {
        [NSApp run];
        return _exitCode;
    }

    // SBShellTarget

    // the window hides in the menu bar
    void onWindowClosing() {
        [_window orderOut:nil];

        // the page and its web content process go away until shown again
        _destroyView();
    }

    void onMenuOpening() {
        [_statusMenu itemWithTag:static_cast<NSInteger>(MenuCommand::MyPlatform)].enabled = _canOpenPlatform();
        [_statusMenu itemWithTag:static_cast<NSInteger>(MenuCommand::Warnings)].enabled = Controller::hasWarnings();
    }

    void onMenuCommand(NSInteger command) { _runMenuCommand(static_cast<MenuCommand>(command)); }

 protected:
    void _wakeUiThread() override {
        dispatch_async_f(dispatch_get_main_queue(), this, [](void* self) { static_cast<MacShell*>(self)->_runPosted(); });
    }

    bool _isUiThread() const override { return NSThread.isMainThread; }

    void _evalNow(const std::string& script) override {
        if (_view) _view->eval(script);
    }

    void _resolveNow(const std::string& id, const std::string& answer) override {
        if (_view) _view->resolve(id, answer);
    }

    void _applyTrayState(TrayState state) override { _statusItem.button.image = menuBarImage(state); }

    void _showNow() override {
        activate();
        if (!_view) _createView();
        [_window makeKeyAndOrderFront:nil];
    }

    bool _askNow(const std::string& title, const std::string& question, Answer byDefault) override {
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

    void _tellNow(const std::string& title, const std::string& text, Severity severity) override {
        NSAlert* const alert = [[NSAlert alloc] init];
        alert.alertStyle = severity == Severity::Error ? NSAlertStyleCritical : NSAlertStyleInformational;
        alert.messageText = toNSString(title);
        alert.informativeText = toNSString(text);
        [alert addButtonWithTitle:toNSString(i18n::tr("OK"))];

        activate();
        [alert runModal];
    }

    void _quitNow(int exitCode) override {
        _exitCode = exitCode;

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

 private:
    SBShellTarget* _target = nil;
    NSWindow* _window = nil;
    NSStatusItem* _statusItem = nil;
    NSMenu* _statusMenu = nil;

    // the webview, while the window is shown
    std::unique_ptr<WebView> _view;
    int _exitCode = 0;

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
        _viewCreated();

        try {
            // fills the window's content
            _view = std::make_unique<WebView>(devTools, (__bridge void*)_window);
        } catch (const std::runtime_error& e) {
            logs::error("UI", "Webview unavailable : {}", e.what());
            _tellNow(APP_NAME, e.what(), Severity::Error);
            return;
        }

        _view->bind("sbQuery", [this](const std::string& id, const std::string& request) { _pageQuery(id, request); });
        _view->bind("sbCommand", [this](const std::string& id, const std::string& request) { _pageCommand(id, request); });

        _view->setHtml(page::html);
    }

    void _destroyView() {
        _viewDestroyed();
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
            exitCode = shell.run();
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

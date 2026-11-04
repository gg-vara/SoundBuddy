// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Platform.h"

#import <AppKit/AppKit.h>

#include <fcntl.h>
#include <sys/file.h>

#include <stdexcept>
#include <system_error>

#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "version.h"

namespace platform {

namespace {

std::filesystem::path toPath(NSString* path) {
    return path ? std::filesystem::path(path.fileSystemRepresentation) : std::filesystem::path();
}

NSString* toNSString(const std::filesystem::path& path) {
    return @(path.c_str());
}

// launchd agent starting the app at login
std::filesystem::path launchAgentFile() {
    return toPath(NSHomeDirectory()) / "Library" / "LaunchAgents" / APP_NAME ".plist";
}

std::filesystem::path applicationFile() {
    return toPath(NSBundle.mainBundle.executablePath);
}

}  // namespace

std::filesystem::path dataDirectory() {
    @autoreleasepool {
        NSString* applicationSupport =
            NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
        const auto* const isolated = isolation();
        const auto directory = isolated ? isolated->dataDirectory : toPath(applicationSupport) / APP_PUBLISHER_ORG / APP_NAME;

        std::error_code error;
        std::filesystem::create_directories(directory, error);

        return directory;
    }
}

bool openPath(const std::filesystem::path& path) {
    if (isIsolated()) {
        logs::info("Platform", "Would open {}", toUtf8(path));
        return true;
    }

    @autoreleasepool {
        return [NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:toNSString(path)]];
    }
}

bool openUrl(const std::string& url) {
    if (isIsolated()) {
        logs::info("Platform", "Would open {}", url);
        return true;
    }

    @autoreleasepool {
        NSURL* target = [NSURL URLWithString:@(url.c_str())];
        return target && [NSWorkspace.sharedWorkspace openURL:target];
    }
}

bool isLaunchingAtStartup() {
    if (isIsolated()) return false;

    @autoreleasepool {
        NSURL* file = [NSURL fileURLWithPath:toNSString(launchAgentFile())];
        NSDictionary* agent = [NSDictionary dictionaryWithContentsOfURL:file error:nil];
        NSArray* arguments = agent[@"ProgramArguments"];
        if (![arguments isKindOfClass:NSArray.class] || arguments.count == 0) return false;

        return [arguments.firstObject isEqual:toNSString(applicationFile())];
    }
}

bool setLaunchingAtStartup(bool enabled) {
    if (isIsolated()) return true;

    const auto file = launchAgentFile();
    std::error_code error;

    if (!enabled) {
        std::filesystem::remove(file, error);
        return !error;
    }

    std::filesystem::create_directories(file.parent_path(), error);
    if (error) return false;

    @autoreleasepool {
        NSDictionary* agent = @{
            @"Label" : @APP_NAME,
            @"ProcessType" : @"Interactive",
            @"ExitTimeOut" : @0,
            @"RunAtLoad" : @YES,
            @"LimitLoadToSessionType" : @"Aqua",
            @"ProgramArguments" : @[ toNSString(applicationFile()) ],
        };

        return [agent writeToURL:[NSURL fileURLWithPath:toNSString(file)] error:nil];
    }
}

std::string_view musicAppName() {
    return i18n::tr("Music");
}

bool claimSingleInstance() {
    // the lock is released by the system when the process ends; the file is never closed before
    static const bool claimed = [] {
        const auto file = dataDirectory() / (APP_NAME ".lock");
        const int descriptor = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        return descriptor >= 0 && flock(descriptor, LOCK_EX | LOCK_NB) == 0;
    }();
    return claimed;
}

std::filesystem::path musicLibraryFile() {
    if (const auto* const isolated = isolation()) {
        if (isolated->libraryFile.empty()) throw std::runtime_error("Isolated run without --library");
        return isolated->libraryFile;
    }

    @autoreleasepool {
        // last directory chosen in Music.app, where its library lies
        const auto value = CFPreferencesCopyAppValue(CFSTR("NSNavLastRootDirectory"), CFSTR("com.apple.Music"));
        NSString* root = CFBridgingRelease(value);

        if (![root isKindOfClass:NSString.class] || root.length == 0) {
            throw std::runtime_error(i18n::format(i18n::tr("%1 XML library location could not be deduced."), { musicAppName() }));
        }

        return toPath(root.stringByExpandingTildeInPath.stringByDeletingLastPathComponent) / "iTunes Music Library.xml";
    }
}

}  // namespace platform

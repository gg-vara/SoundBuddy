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

#include "Platform.h"

#import <AppKit/AppKit.h>

#include <fcntl.h>
#include <sys/file.h>

#include <stdexcept>
#include <system_error>

#include "src/i18n/Translations.h"
#include "version.h"

namespace platform {

namespace {

std::filesystem::path toPath(NSString * path) {
    return path ? std::filesystem::path(path.fileSystemRepresentation) : std::filesystem::path();
}

NSString * toNSString(const std::filesystem::path& path) {
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
        NSString * applicationSupport = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
        const auto directory = toPath(applicationSupport) / APP_PUBLISHER_ORG / APP_NAME;

        std::error_code error;
        std::filesystem::create_directories(directory, error);

        return directory;
    }
}

bool openPath(const std::filesystem::path& path) {
    @autoreleasepool {
        return [NSWorkspace.sharedWorkspace openURL:[NSURL fileURLWithPath:toNSString(path)]];
    }
}

bool openUrl(const std::string& url) {
    @autoreleasepool {
        NSURL * target = [NSURL URLWithString:@(url.c_str())];
        return target && [NSWorkspace.sharedWorkspace openURL:target];
    }
}

bool isLaunchingAtStartup() {
    @autoreleasepool {
        NSURL * file = [NSURL fileURLWithPath:toNSString(launchAgentFile())];
        NSDictionary * agent = [NSDictionary dictionaryWithContentsOfURL:file error:nil];
        NSArray * arguments = agent[@"ProgramArguments"];
        if (![arguments isKindOfClass:NSArray.class] || arguments.count == 0) return false;

        return [arguments.firstObject isEqual:toNSString(applicationFile())];
    }
}

bool setLaunchingAtStartup(bool enabled) {
    const auto file = launchAgentFile();
    std::error_code error;

    if (!enabled) {
        std::filesystem::remove(file, error);
        return !error;
    }

    std::filesystem::create_directories(file.parent_path(), error);
    if (error) return false;

    @autoreleasepool {
        NSDictionary * agent = @{
            @"Label": @APP_NAME,
            @"ProcessType": @"Interactive",
            @"ExitTimeOut": @0,
            @"RunAtLoad": @YES,
            @"LimitLoadToSessionType": @"Aqua",
            @"ProgramArguments": @[ toNSString(applicationFile()) ],
        };

        return [agent writeToURL:[NSURL fileURLWithPath:toNSString(file)] error:nil];
    }
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
    @autoreleasepool {
        // last directory chosen in Music.app, where its library lies
        const auto value = CFPreferencesCopyAppValue(CFSTR("NSNavLastRootDirectory"), CFSTR("com.apple.Music"));
        NSString * root = CFBridgingRelease(value);

        if (![root isKindOfClass:NSString.class] || root.length == 0) {
            throw std::runtime_error(i18n::format(i18n::tr("%1 XML library location could not be deduced."), { i18n::tr("Music") }));
        }

        return toPath(root.stringByExpandingTildeInPath.stringByDeletingLastPathComponent) / "iTunes Music Library.xml";
    }
}

}  // namespace platform

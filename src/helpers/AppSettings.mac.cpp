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

#include "AppSettings.h"

#include <CoreFoundation/CoreFoundation.h>

#include <cctype>
#include <string>
#include <vector>

namespace {

// a Core Foundation object, released once destroyed
template <typename Ref>
class Owned {
 public:
    explicit Owned(Ref ref) :
        _ref(ref) {}
    ~Owned() {
        if (_ref) CFRelease(_ref);
    }
    Owned(const Owned&) = delete;
    void operator=(const Owned&) = delete;

    Ref get() const { return _ref; }

 private:
    Ref _ref;
};

Owned<CFStringRef> toCFString(const std::string& text) {
    return Owned<CFStringRef> { CFStringCreateWithCString(nullptr, text.c_str(), kCFStringEncodingUTF8) };
}

// "value" as UTF-8, if a string
std::optional<std::string> toString(CFPropertyListRef value) {
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) return std::nullopt;
    const auto text = static_cast<CFStringRef>(value);

    const auto size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(text), kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(static_cast<std::size_t>(size));
    if (!CFStringGetCString(text, buffer.data(), size, kCFStringEncodingUTF8)) return std::nullopt;
    return std::string { buffer.data() };
}

// QSettings stored booleans as such, or as "true" / "false" texts
bool toBoolean(CFPropertyListRef value) {
    if (value && CFGetTypeID(value) == CFBooleanGetTypeID()) return CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    return toString(value) == "true";
}

}  // namespace

// QSettings named it after the organization, as a domain, then the application : com.<organization>.<application>
std::string AppSettings::legacyDomain() {
    std::string organization;
    for (const char c : std::string_view { APP_PUBLISHER_ORG }) {
        if (std::isalnum(static_cast<unsigned char>(c))) organization += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return "com." + organization + "." APP_NAME;
}

// stored by QSettings in the preferences of its domain
std::optional<settings::Settings> AppSettings::_readLegacySettings() {
    const auto domain = toCFString(legacyDomain());
    const auto value = [&domain](const char* key) {
        return Owned<CFPropertyListRef> { CFPreferencesCopyAppValue(toCFString(key).get(), domain.get()) };
    };

    const auto hostUrl = value("PlatformHostUrl");
    const auto username = value("PlatformUsername");
    const auto password = value("PlatformPassword");
    const auto autoRunShout = value("AutoRunShout");
    const auto runAtStartup = value("MustRunAtStartup");
    if (!hostUrl.get() && !username.get() && !password.get() && !autoRunShout.get() && !runAtStartup.get()) return std::nullopt;

    return settings::Settings {
        .platformHostUrl = toString(hostUrl.get()).value_or(""),
        .platformUsername = toString(username.get()).value_or(""),
        .platformPassword = toString(password.get()).value_or(""),
        .autoRunShout = toBoolean(autoRunShout.get()),
        .runAtStartup = toBoolean(runAtStartup.get()),
    };
}

#endif

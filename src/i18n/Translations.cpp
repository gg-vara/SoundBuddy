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

#include "Translations.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <format>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>

#ifdef _WIN32
    #include <windows.h>
#elif defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#endif

#include "src/logs/Logs.h"

namespace i18n {

namespace {

using Table = std::unordered_map<std::string_view, std::string_view>;

// English source text => translation
const Table frenchTable {
    #include "fr.inc"
};

// language tag of the user interface, like "fr-FR", lowercase
std::string systemLanguageTag() {
#ifdef _WIN32
    ULONG count = 0;
    ULONG size = 0;
    if ((GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, nullptr, &size) == 0) || size == 0) return {};

    // double-null terminated list : the first one is the preferred one
    std::wstring languages(size, L'\0');
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, languages.data(), &size) == 0) return {};

    std::string tag;
    for (const wchar_t c : languages) {
        if (c == L'\0') break;
        tag += static_cast<char>(c < 128 ? std::tolower(c) : '?');
    }
    return tag;
#elif defined(__APPLE__)
    std::string tag;
    const auto languages = CFLocaleCopyPreferredLanguages();
    if (languages && CFArrayGetCount(languages) > 0) {
        const auto first = static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages, 0));
        char buffer[64];
        if (CFStringGetCString(first, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            for (const char * c = buffer; *c; ++c) tag += static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
        }
    }
    if (languages) CFRelease(languages);
    return tag;
#else
    return {};
#endif
}

Language detectLanguage() {
    const auto tag = systemLanguageTag();
    if (tag.starts_with("fr")) return Language::French;
    return Language::English;
}

// detected on first use, unless set before
std::atomic<std::optional<Language>>& currentLanguage() {
    static std::atomic<std::optional<Language>> language;
    return language;
}

#ifdef _DEBUG
// reports each untranslated text once, to spot missing table entries during development
void reportMissing(std::string_view source) {
    static std::mutex mutex;
    static std::set<std::string, std::less<>> reported;

    const std::scoped_lock lock { mutex };
    if (reported.insert(std::string { source }).second) {
        logs::debug("i18n", "Missing translation : \"{}\"", source);
    }
}
#endif

}  // namespace

Language language() {
    auto language = currentLanguage().load();
    if (!language) {
        language = detectLanguage();
        currentLanguage().store(language);
    }
    return *language;
}

void setLanguage(Language language) {
    currentLanguage().store(language);
}

std::string_view languageCode() {
    switch (language()) {
        case Language::French: return "fr";
        case Language::English: break;
    }
    return "en";
}

std::string formatDataSize(std::uint64_t bytes) {
    const auto french = language() == Language::French;
    constexpr std::array<std::string_view, 4> englishUnits { "B", "KB", "MB", "GB" };
    constexpr std::array<std::string_view, 4> frenchUnits { "o", "Ko", "Mo", "Go" };

    auto value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024 && unit + 1 < englishUnits.size()) {
        value /= 1024;
        ++unit;
    }

    auto text = unit == 0 ? std::to_string(bytes) : std::format("{:.2f}", value);
    if (french) std::ranges::replace(text, '.', ',');

    return std::format("{} {}", text, french ? frenchUnits[unit] : englishUnits[unit]);
}

std::string_view tr(std::string_view source) {
    if (language() == Language::English) return source;

    const auto found = frenchTable.find(source);
    if (found != frenchTable.end()) return found->second;

#ifdef _DEBUG
    reportMissing(source);
#endif

    return source;
}

std::string format(std::string_view text, std::initializer_list<std::string_view> args) {
    std::string result;
    result.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        // %1 to %9
        if (text[i] == '%' && i + 1 < text.size() && text[i + 1] >= '1' && text[i + 1] <= '9') {
            const auto index = static_cast<std::size_t>(text[i + 1] - '1');
            if (index < args.size()) {
                result += *(args.begin() + index);
                ++i;
                continue;
            }
        }

        result += text[i];
    }

    return result;
}

}  // namespace i18n

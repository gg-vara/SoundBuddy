// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// src/settings, and AppSettings : settings file, import of the QSettings values of previous versions

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "Check.h"
#include "TestData.h"
#include "src/helpers/AppSettings.h"
#include "src/helpers/Platform.h"
#include "src/settings/Settings.h"
#include "version.h"

namespace {

std::string readAll(const std::filesystem::path& file) {
    std::ifstream stream { file, std::ios::binary };
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

// settings of previous versions, as QSettings stored them : texts, and booleans as "true" / "false"
namespace legacy {

#ifdef _WIN32
const wchar_t* const organizationKey = L"Software\\" APP_PUBLISHER_ORG;
const wchar_t* const applicationKey = L"Software\\" APP_PUBLISHER_ORG "\\" APP_NAME;

void write(const wchar_t* name, const std::string& value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, applicationKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    const auto wide = platform::widen(value);
    RegSetValueExW(
        key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(wide.c_str()), static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t))
    );
    RegCloseKey(key);
}

void clear() {
    RegDeleteTreeW(HKEY_CURRENT_USER, organizationKey);
    RegDeleteKeyW(HKEY_CURRENT_USER, organizationKey);
}
#else
// in the preferences domain of the test identity (see AppSettings::legacyDomain())
void set(const char* name, CFPropertyListRef value) {
    auto* const domain = CFStringCreateWithCString(nullptr, AppSettings::legacyDomain().c_str(), kCFStringEncodingUTF8);
    auto* const key = CFStringCreateWithCString(nullptr, name, kCFStringEncodingUTF8);
    CFPreferencesSetAppValue(key, value, domain);
    CFPreferencesAppSynchronize(domain);
    CFRelease(key);
    CFRelease(domain);
}

void write(const char* name, const std::string& value) {
    if (value == "true" || value == "false") {
        set(name, value == "true" ? kCFBooleanTrue : kCFBooleanFalse);
        return;
    }
    auto* const text = CFStringCreateWithCString(nullptr, value.c_str(), kCFStringEncodingUTF8);
    set(name, text);
    CFRelease(text);
}

void clear() {
    for (const char* name : { "PlatformHostUrl", "PlatformUsername", "PlatformPassword", "AutoRunShout", "MustRunAtStartup" })
        set(name, nullptr);
}
#endif

}  // namespace legacy

#ifdef _WIN32
#define LEGACY_NAME(name) L##name
#else
#define LEGACY_NAME(name) name
#endif

}  // namespace

int main() {
    const auto file = testdata::scratchDirectory("settings é") / "sub" / "settings.json";

    CHECK(!settings::load(file), "missing file : nothing");

    settings::Settings values { .platformHostUrl = "https://host.example",
                                .platformUsername = "j\xC3\xA9r\xC3\xB4me \"q\"",
                                .platformPassword = "p\\w\n",
                                .autoRunShout = true,
                                .runAtStartup = false };
    CHECK(settings::save(file, values), "save creates directories");
    CHECK(!std::filesystem::exists(file.parent_path() / "settings.json.tmp"), "no temporary file left");

    auto loaded = settings::load(file);
    CHECK(
        loaded && loaded->platformHostUrl == values.platformHostUrl && loaded->platformUsername == values.platformUsername &&
            loaded->platformPassword == values.platformPassword && loaded->autoRunShout && !loaded->runAtStartup,
        "round trip : UTF-8, escapes"
    );

    values.runAtStartup = true;
    values.platformHostUrl = "x";
    const auto overwritten = settings::save(file, values) ? settings::load(file) : std::nullopt;
    CHECK(overwritten && overwritten->runAtStartup && overwritten->platformHostUrl == "x", "overwrite");

    {
        std::ofstream { file, std::ios::binary } << R"({"autoRunShout": true, "platformUsername": 42, "unknown": []})";
    }
    loaded = settings::load(file);
    CHECK(
        loaded && loaded->autoRunShout && loaded->platformUsername.empty() && !loaded->runAtStartup,
        "partial file : defaults for missing values and wrong types"
    );

    {
        std::ofstream { file, std::ios::binary } << "{ not json";
    }
    CHECK(!settings::load(file), "invalid JSON : nothing");
    {
        std::ofstream { file, std::ios::binary } << "[1,2]";
    }
    CHECK(!settings::load(file), "not an object : nothing");

    // AppSettings, under the test identity
    testdata::removeDataDirectory();
    const auto dataFile = platform::dataDirectory() / "settings.json";

    legacy::clear();
    {
        AppSettings appSettings;
        CHECK(appSettings.values().platformHostUrl.empty() && !std::filesystem::exists(dataFile), "nothing to import : defaults");
    }

    legacy::write(LEGACY_NAME("PlatformHostUrl"), "https://legacy.example");
    legacy::write(LEGACY_NAME("PlatformUsername"), "l\xC3\xA9gacy");
    legacy::write(LEGACY_NAME("PlatformPassword"), "secret");
    legacy::write(LEGACY_NAME("AutoRunShout"), "true");
    legacy::write(LEGACY_NAME("MustRunAtStartup"), "false");
    {
        AppSettings appSettings;
        const auto& imported = appSettings.values();
        CHECK(
            imported.platformHostUrl == "https://legacy.example" && imported.platformUsername == "l\xC3\xA9gacy" &&
                imported.platformPassword == "secret" && imported.autoRunShout && !imported.runAtStartup,
            "previous versions settings imported"
        );
        CHECK(std::filesystem::exists(dataFile), "settings file created by the import");
        appSettings.startSession({});
        CHECK(
            appSettings.getConnectivityInfos().areOK &&
                appSettings.getConnectivityInfos().getPlaformHomeUrl() == "https://legacy.example/u/l\xC3\xA9gacy",
            "connectivity infos"
        );
    }
    {
        legacy::write(LEGACY_NAME("PlatformHostUrl"), "https://changed.example");

        AppSettings appSettings;
        CHECK(appSettings.values().platformHostUrl == "https://legacy.example", "the settings file wins once created");

        appSettings.values().autoRunShout = false;
        CHECK(appSettings.save() && !AppSettings().values().autoRunShout, "saved, then reloaded");
    }

#ifdef _WIN32
    // QSettings doubles the '@' starting a text
    std::filesystem::remove(dataFile);
    legacy::write(L"PlatformPassword", "@@pass");
    CHECK(AppSettings().values().platformPassword == "@pass", "escaped '@' of QSettings");
#endif

    legacy::clear();
    testdata::removeDataDirectory();
    testdata::removeScratchDirectories();
    return check::summary();
}

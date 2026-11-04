// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// --data-dir and its promise : a run that stays away from the session's real data

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Check.h"
#include "TestData.h"
#include "src/helpers/AppSettings.h"
#include "src/helpers/Options.h"
#include "src/helpers/Platform.h"
#include "version.h"

namespace {

void checkOptions() {
    const auto parsed = [](const std::vector<std::string>& arguments) { return options::parse(arguments); };

    CHECK(parsed({}).error.empty() && !parsed({}).options.dataDirectory && !parsed({}).options.headless, "no option");
    CHECK(parsed({ "--veloapp-install", "1.0.0" }).error.empty(), "the options of others are left alone");
    CHECK(!parsed({ "--headless" }).error.empty(), "--headless alone would act on the real settings");
    CHECK(!parsed({ "--library", "library.xml" }).error.empty(), "--library alone would act on the real settings");
    CHECK(!parsed({ "--data-dir" }).error.empty(), "--data-dir without its path");

    const auto full = parsed({ "--headless", "--data-dir", "folder", "--library", "library.xml" });
    CHECK(full.error.empty() && full.options.headless, "all options");
    CHECK(full.options.dataDirectory && full.options.dataDirectory->is_absolute(), "paths are made absolute");
}

#ifdef _WIN32
// the values QSettings left for previous versions, which an isolated run must not read
void writeLegacySetting(const std::wstring& key) {
    const std::wstring value = L"https://real-platform.example";
    RegSetKeyValueW(
        HKEY_CURRENT_USER, key.c_str(), L"PlatformHostUrl", REG_SZ, value.c_str(), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))
    );
}
#endif

void checkIsolation() {
    const auto real = platform::dataDirectory();
    const auto directory = testdata::scratchDirectory("isolation") / "data";
    const auto library = directory.parent_path() / "library.xml";

#ifdef _WIN32
    const std::wstring legacyKey = L"Software\\" APP_PUBLISHER_ORG "\\" APP_NAME;
    writeLegacySetting(legacyKey);
#endif

    CHECK(!platform::isIsolated() && platform::isolation() == nullptr, "not isolated by default");

    platform::isolate({ .dataDirectory = directory, .libraryFile = {} });

    CHECK(platform::isIsolated(), "isolated");
    CHECK(platform::dataDirectory() == directory && std::filesystem::is_directory(directory), "data in the given folder, created");
    CHECK(platform::dataDirectory() != real, "not in the real one");

    const AppSettings settings;
    CHECK(settings.values().platformHostUrl.empty(), "settings of previous versions are not imported");

    CHECK(platform::setLaunchingAtStartup(/*enabled=*/true) && !platform::isLaunchingAtStartup(), "start at login stays off");
    CHECK(platform::openUrl("https://example.invalid") && platform::openPath(directory), "nothing is opened in other apps");
    CHECK(platform::claimSingleInstance(), "runs beside the real app");

    bool refused = false;
    try {
        platform::musicLibraryFile();
    } catch (const std::runtime_error&) {
        refused = true;
    }
    CHECK(refused, "without --library, the real library is left alone");

    platform::isolate({ .dataDirectory = directory, .libraryFile = library });
    CHECK(platform::musicLibraryFile() == library, "the given library");

#ifdef _WIN32
    RegDeleteTreeW(HKEY_CURRENT_USER, legacyKey.c_str());
#endif
}

}  // namespace

int main() {
    checkOptions();
    checkIsolation();

    testdata::removeScratchDirectories();
    return check::summary();
}

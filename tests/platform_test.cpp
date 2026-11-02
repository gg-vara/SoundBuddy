// src/helpers/Platform.h : data directory, UTF-8 paths, launch at login, single instance, music library.
// Writes only under the test identity (tests/support/version.h).

#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Check.h"
#include "TestData.h"
#include "src/helpers/Platform.h"
#include "version.h"

namespace {

#ifdef _WIN32
// exit code of this test program, run again with the "claim" argument
int runClaimingChild() {
    std::array<wchar_t, MAX_PATH> executable {};
    GetModuleFileNameW(nullptr, executable.data(), MAX_PATH);

    std::wstring command = std::wstring(L"\"") + executable.data() + L"\" claim";
    STARTUPINFOW startup { .cb = sizeof(startup) };
    PROCESS_INFORMATION process {};
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process) == 0) return -1;

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return static_cast<int>(code);
}

std::wstring registeredStartupCommand() {
    std::array<wchar_t, 1024> value {};
    auto size = static_cast<DWORD>(value.size() * sizeof(wchar_t));
    const auto result = RegGetValueW(
        HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Run)", L"" APP_NAME, RRF_RT_REG_SZ, nullptr, value.data(), &size
    );
    if (result != ERROR_SUCCESS) return {};
    return value.data();
}
#endif

}  // namespace

int main(int argc, char** argv) {
    // child process : 0 if it got the single instance claim
    if (argc > 1 && std::string(argv[1]) == "claim") return platform::claimSingleInstance() ? 0 : 1;

    // data directory
    const auto data = platform::dataDirectory();
    CHECK(
        std::filesystem::is_directory(data) && data.filename() == APP_NAME && data.parent_path().filename() == APP_PUBLISHER_ORG,
        "data directory created, under the test identity"
    );

    // UTF-8
    const std::string utf8 = "D:/Musique/caf\xC3\xA9/\xE9\x9F\xB3.xml";
    CHECK(platform::toUtf8(platform::fromUtf8(utf8)) == utf8, "UTF-8 path round trip");
#ifdef _WIN32
    CHECK(platform::fromUtf8(utf8).wstring() == L"D:/Musique/caf\u00e9/\u97f3.xml", "UTF-8 path to UTF-16");
    const std::wstring wide = L"Caf\u00e9 \u97f3\U0001F3B5";
    CHECK(platform::narrow(wide) == "Caf\xC3\xA9 \xE9\x9F\xB3\xF0\x9F\x8E\xB5", "narrow, including a surrogate pair");
    CHECK(
        platform::widen(platform::narrow(wide)) == wide && platform::narrow(L"").empty() && platform::widen("").empty(),
        "widen round trip, empty texts"
    );
#endif

    // launch at login
    CHECK(platform::setLaunchingAtStartup(false) && !platform::isLaunchingAtStartup(), "not launching at startup");
    CHECK(platform::setLaunchingAtStartup(true) && platform::isLaunchingAtStartup(), "enable startup launch");
#ifdef _WIN32
    std::array<wchar_t, MAX_PATH> executable {};
    GetModuleFileNameW(nullptr, executable.data(), MAX_PATH);
    CHECK(registeredStartupCommand() == std::wstring(L"\"") + executable.data() + L"\"", "registered command : the quoted executable path");
#endif
    CHECK(platform::setLaunchingAtStartup(true) && platform::isLaunchingAtStartup(), "enable twice");
    CHECK(platform::setLaunchingAtStartup(false) && !platform::isLaunchingAtStartup(), "disable startup launch");
    CHECK(platform::setLaunchingAtStartup(false), "disable twice");

    // single instance
#ifdef _WIN32
    CHECK(runClaimingChild() == 0, "alone : claimed");
    CHECK(platform::claimSingleInstance() && platform::claimSingleInstance(), "claimed, twice by the same process");
    CHECK(runClaimingChild() == 1, "another process is refused meanwhile");
#endif

    // music library : only where iTunes / Music is set up
    try {
        const auto library = platform::musicLibraryFile();
        std::printf("  music library : %s\n", platform::toUtf8(library).c_str());
        CHECK(library.filename() == "iTunes Music Library.xml", "music library deduced");
    } catch (const std::runtime_error& e) {
        std::printf("  SKIP  music library : %s\n", e.what());
    }

    testdata::removeDataDirectory();
    return check::summary();
}

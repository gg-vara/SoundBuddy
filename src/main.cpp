// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <Velopack.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include "src/helpers/Options.h"
#include "src/helpers/Platform.h"
#include "src/logs/Logs.h"
#include "src/ui/Headless.h"
#include "src/ui/Run.h"
#include "version.h"

namespace {

logs::Level toLevel(std::string_view velopackLevel) {
    if (velopackLevel.starts_with("err") || velopackLevel.starts_with("Err")) return logs::Level::Error;
    if (velopackLevel.starts_with("warn") || velopackLevel.starts_with("Warn")) return logs::Level::Warning;
    if (velopackLevel.starts_with("info") || velopackLevel.starts_with("Info")) return logs::Level::Info;
    return logs::Level::Debug;
}

// installer and updates
void logVelopackMessage(void* /*userData*/, const char* level, const char* message) {
    logs::write(toLevel(level ? level : ""), "Velopack", message ? message : "");
}

// last words before quitting on an unhandled exception : never throws
void logFatalError(const char* what) noexcept {
    try {
        logs::error("App", "Fatal error : {}", what);
        logs::stop();
    } catch (...) {  // NOLINT(bugprone-empty-catch) : nothing left to report to
    }
}

// the arguments, in UTF-8, without the name of the program
std::vector<std::string> arguments([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
    std::vector<std::string> result;

#ifdef _WIN32
    // "argv" is in the system's code page
    int count = 0;
    auto* const wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wide != nullptr) {
        for (const auto* const argument : std::span(wide, static_cast<std::size_t>(count)).subspan(1)) {
            result.push_back(platform::narrow(argument));
        }
        LocalFree(static_cast<void*>(wide));
    }
#else
    for (const auto* const argument : std::span(argv, static_cast<std::size_t>(argc)).subspan(1)) result.emplace_back(argument);
#endif

    return result;
}

int run(int argc, char** argv) {
    const auto [options, error] = options::parse(arguments(argc, argv));
    if (!error.empty()) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 2;
    }

    if (options.dataDirectory) {
        platform::isolate(
            { .dataDirectory = *options.dataDirectory, .libraryFile = options.libraryFile.value_or(std::filesystem::path {}) }
        );
    }

    logs::start(platform::dataDirectory() / "logs" / "soundbuddy.log");

    // must run first : handles install / update / uninstall hooks, and may exit the process right away.
    // Not when isolated, as it also applies the updates already downloaded
    if (!platform::isIsolated()) Velopack::VelopackApp::Build().SetLogger(&logVelopackMessage, nullptr).Run();

    logs::info("App", "{} {} started", APP_NAME, APP_CURRENT_VERSION);

    // prevent multiples instances
    if (!platform::claimSingleInstance()) {
        logs::info("App", "Already running, exiting");
        return 1;
    }

    const auto exitCode = options.headless ? ui::runHeadless() : ui::run(argc, argv);
    logs::info("App", "Exiting with code {}", exitCode);
    logs::stop();
    return exitCode;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        logFatalError(e.what());
    } catch (...) {
        logFatalError("unknown exception");
    }
    return EXIT_FAILURE;
}

#ifdef _WIN32
// entry point of release builds, without a console
int WINAPI WinMain(HINSTANCE /*instance*/, HINSTANCE /*previous*/, LPSTR /*commandLine*/, int /*show*/) {
    return main(__argc, __argv);
}
#endif

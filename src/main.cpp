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

#include <Velopack.hpp>

#include <cstdlib>
#include <exception>
#include <string_view>

#include "src/helpers/Platform.h"
#include "src/logs/Logs.h"
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

int run(int argc, char** argv) {
    logs::start(platform::dataDirectory() / "logs" / "soundbuddy.log");

    // must run first : handles install / update / uninstall hooks, and may exit the process right away
    Velopack::VelopackApp::Build().SetLogger(&logVelopackMessage, nullptr).Run();

    logs::info("App", "{} {} started", APP_NAME, APP_CURRENT_VERSION);

    // prevent multiples instances
    if (!platform::claimSingleInstance()) {
        logs::info("App", "Already running, exiting");
        return 1;
    }

    const auto exitCode = ui::run(argc, argv);
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

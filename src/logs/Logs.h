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

#pragma once

#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

// Application logs, usable from any thread.
// Lines go to a log file once started, and to the debugger output (plus stderr in debug builds).
namespace logs {

enum class Level {
    Debug,  // development details, left out of release builds
    Info,
    Warning,
    Error
};

// starts writing to "file", creating its directory if needed.
// Past 1 MB, the file is renamed with a ".1" suffix (replacing the previous one), and a new one begins
void start(const std::filesystem::path& file);

// closes the log file : lines then only go to the debugger output
void stop();

// writes a line, "source" naming the part of the app it comes from
void write(Level level, std::string_view source, std::string_view message);

template <typename... Args>
void debug([[maybe_unused]] std::string_view source, [[maybe_unused]] std::format_string<Args...> format, [[maybe_unused]] Args&&... args) {
#ifdef _DEBUG
    write(Level::Debug, source, std::format(format, std::forward<Args>(args)...));
#endif
}

template <typename... Args>
void info(std::string_view source, std::format_string<Args...> format, Args&&... args) {
    write(Level::Info, source, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void warning(std::string_view source, std::format_string<Args...> format, Args&&... args) {
    write(Level::Warning, source, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::string_view source, std::format_string<Args...> format, Args&&... args) {
    write(Level::Error, source, std::format(format, std::forward<Args>(args)...));
}

}  // namespace logs

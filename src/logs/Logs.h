// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

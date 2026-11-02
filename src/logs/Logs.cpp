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

#include "Logs.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

#include "src/helpers/Timestamps.hpp"

namespace logs {

namespace {

constexpr std::uintmax_t maxFileSize = std::uintmax_t { 1024 } * 1024;

// the log file, shared by all threads
struct LogFile {
    std::mutex mutex;
    std::filesystem::path path;
    std::ofstream stream;
    std::uintmax_t size = 0;
};

LogFile& logFile() {
    static LogFile instance;
    return instance;
}

std::string_view label(Level level) {
    switch (level) {
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warning:
            return "WARNING";
        case Level::Error:
            return "ERROR";
    }
    return "?";
}

void writeToDebugOutputs(const std::string& line) {
#ifdef _DEBUG
    std::fputs(line.c_str(), stderr);
#endif

#ifdef _WIN32
    const auto size = MultiByteToWideChar(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, line.data(), static_cast<int>(line.size()), wide.data(), size);
    OutputDebugStringW(wide.c_str());
#endif
}

// with the mutex held
void open(LogFile& log) {
    log.stream.open(log.path, std::ios::binary | std::ios::app);

    std::error_code error;
    log.size = std::filesystem::file_size(log.path, error);
    if (error) log.size = 0;
}

// with the mutex held : soundbuddy.log becomes soundbuddy.1.log
void rotate(LogFile& log) {
    log.stream.close();

    auto previous = log.path;
    previous.replace_extension();
    previous += ".1";
    previous += log.path.extension();

    std::error_code error;
    std::filesystem::rename(log.path, previous, error);

    open(log);
}

}  // namespace

void start(const std::filesystem::path& file) {
    auto& log = logFile();
    const std::scoped_lock lock { log.mutex };

    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);

    log.stream.close();
    log.path = file;
    open(log);
}

void stop() {
    auto& log = logFile();
    const std::scoped_lock lock { log.mutex };
    log.stream.close();
}

void write(Level level, std::string_view source, std::string_view message) {
#ifndef _DEBUG
    if (level == Level::Debug) return;
#endif

    const auto line = std::format("{} [{}] [{}] {}\n", timestamps::localPrecise(), label(level), source, message);

    auto& log = logFile();
    const std::scoped_lock lock { log.mutex };

    writeToDebugOutputs(line);

    if (!log.stream.is_open()) return;
    if (log.size > 0 && log.size + line.size() > maxFileSize) rotate(log);

    // flushed right away, so that nothing is lost on crash
    log.stream.write(line.data(), static_cast<std::streamsize>(line.size()));
    log.stream.flush();
    log.size += line.size();
}

}  // namespace logs

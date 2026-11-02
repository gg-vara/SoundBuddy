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

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "src/helpers/Timestamps.hpp"

namespace logs {

namespace {

constexpr std::uintmax_t MAX_FILE_SIZE = 1024 * 1024;

std::mutex mutex;
std::filesystem::path currentFile;
std::ofstream stream;
std::uintmax_t currentSize = 0;

std::string_view label(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info: return "INFO";
        case Level::Warning: return "WARNING";
        case Level::Error: return "ERROR";
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
void openCurrentFile() {
    stream.open(currentFile, std::ios::binary | std::ios::app);

    std::error_code error;
    currentSize = std::filesystem::file_size(currentFile, error);
    if (error) currentSize = 0;
}

// with the mutex held : soundbuddy.log becomes soundbuddy.1.log
void rotate() {
    stream.close();

    auto previous = currentFile;
    previous.replace_extension();
    previous += ".1";
    previous += currentFile.extension();

    std::error_code error;
    std::filesystem::rename(currentFile, previous, error);

    openCurrentFile();
}

}  // namespace

void start(const std::filesystem::path& file) {
    std::lock_guard lock { mutex };

    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);

    stream.close();
    currentFile = file;
    openCurrentFile();
}

void write(Level level, std::string_view source, std::string_view message) {
#ifndef _DEBUG
    if (level == Level::Debug) return;
#endif

    const auto line = std::format("{} [{}] [{}] {}\n", timestamps::localPrecise(), label(level), source, message);

    std::lock_guard lock { mutex };

    writeToDebugOutputs(line);

    if (!stream.is_open()) return;
    if (currentSize > 0 && currentSize + line.size() > MAX_FILE_SIZE) rotate();

    // flushed right away, so that nothing is lost on crash
    stream.write(line.data(), static_cast<std::streamsize>(line.size()));
    stream.flush();
    currentSize += line.size();
}

}  // namespace logs

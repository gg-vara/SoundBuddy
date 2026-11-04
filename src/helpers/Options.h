// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Command line of the app, to run it apart from the session's real data :
//   --data-dir <folder>  keeps to that folder (see platform::isolate)
//   --library <file>     reads that iTunes XML library, with --data-dir
//   --headless           no window : driven through its standard input and output (see ui::runHeadless), with --data-dir
namespace options {

struct Options {
    std::optional<std::filesystem::path> dataDirectory;
    std::optional<std::filesystem::path> libraryFile;
    bool headless = false;
};

struct Parsed {
    Options options;
    std::string error;  // empty if none
};

// "arguments" are UTF-8, without the name of the program. Others than the above are left alone, as Velopack's
Parsed parse(const std::vector<std::string>& arguments);

}  // namespace options

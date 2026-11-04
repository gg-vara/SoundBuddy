// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Options.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "Platform.h"

namespace options {

namespace {

std::filesystem::path absolutePath(const std::string& utf8) {
    std::error_code error;
    const auto path = platform::fromUtf8(utf8);
    const auto absolute = std::filesystem::absolute(path, error);
    return error ? path : absolute;
}

}  // namespace

Parsed parse(const std::vector<std::string>& arguments) {
    Parsed parsed;
    auto& options = parsed.options;

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto& argument = arguments[index];

        if (argument == "--headless") {
            options.headless = true;
        } else if (argument == "--data-dir" || argument == "--library") {
            if (++index == arguments.size()) {
                parsed.error = argument + " needs a path";
                return parsed;
            }
            (argument == "--data-dir" ? options.dataDirectory : options.libraryFile) = absolutePath(arguments[index]);
        }
    }

    // these would otherwise act on the session's real settings
    if (!options.dataDirectory) {
        if (options.headless) parsed.error = "--headless needs --data-dir";
        if (options.libraryFile) parsed.error = "--library needs --data-dir";
    }

    return parsed;
}

}  // namespace options

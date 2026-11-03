// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Platform.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace platform {

std::string toUtf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return { text.begin(), text.end() };
}

std::filesystem::path fromUtf8(std::string_view text) {
    return std::u8string { text.begin(), text.end() };
}

}  // namespace platform

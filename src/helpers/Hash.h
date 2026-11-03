// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <string>

namespace hash {

// MD5 of the content of "file", as lowercase hexadecimal; empty if it cannot be read.
// A footprint identifying audio files, not a cryptographic use.
std::string md5File(const std::filesystem::path& file);

}  // namespace hash

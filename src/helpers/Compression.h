// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace compression {

// "data" compressed by the system, in a format SoundVitrine decodes : an MSZIP buffer of the
// Compression API on Windows (which has no zlib API), zlib on macOS. Empty on failure
std::string compress(std::string_view data);

}  // namespace compression

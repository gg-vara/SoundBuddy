// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef __APPLE__

#include "Compression.h"

// part of the system (libz of the macOS SDK)
#include <zlib.h>

namespace compression {

std::string compress(std::string_view data) {
    auto size = compressBound(data.size());
    std::string compressed(size, '\0');

    const auto result = compress2(
        reinterpret_cast<Bytef*>(compressed.data()), &size, reinterpret_cast<const Bytef*>(data.data()), data.size(), Z_DEFAULT_COMPRESSION
    );
    if (result != Z_OK) return {};

    compressed.resize(size);
    return compressed;
}

}  // namespace compression

#endif

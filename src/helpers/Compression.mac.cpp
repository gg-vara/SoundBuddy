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

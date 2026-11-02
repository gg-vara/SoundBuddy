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

#ifdef _WIN32

#include "Compression.h"

#include <windows.h>
#include <compressapi.h>

namespace compression {

std::string compress(std::string_view data) {
    COMPRESSOR_HANDLE compressor = nullptr;
    if (CreateCompressor(COMPRESS_ALGORITHM_MSZIP, nullptr, &compressor) == 0) return {};

    // the API takes a non-const pointer, but does not write to the input
    auto* const input = const_cast<char*>(data.data());

    // first call : the size of the compressed buffer
    SIZE_T size = 0;
    std::string compressed;
    if ((Compress(compressor, input, data.size(), nullptr, 0, &size) == 0) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        compressed.resize(size);
        if (Compress(compressor, input, data.size(), compressed.data(), compressed.size(), &size) != 0) {
            compressed.resize(size);
        } else {
            compressed.clear();
        }
    }

    CloseCompressor(compressor);
    return compressed;
}

}  // namespace compression

#endif

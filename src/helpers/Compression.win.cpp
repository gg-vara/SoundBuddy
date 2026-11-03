// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <string_view>
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

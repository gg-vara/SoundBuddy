// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Hash.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#endif

namespace hash {

namespace {

using Digest = std::array<unsigned char, 16>;

// passes the content of "file" to "consume", chunk by chunk; false if it cannot be read entirely
template <typename Consume>
bool readChunks(const std::filesystem::path& file, const Consume& consume) {
    std::ifstream stream { file, std::ios::binary };
    if (!stream) return false;

    std::vector<char> buffer(std::size_t { 64 } * 1024);
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        if (count > 0 && !consume(buffer.data(), static_cast<std::size_t>(count))) return false;
    }

    return stream.eof();
}

#ifdef _WIN32

std::optional<Digest> md5(const std::filesystem::path& file) {
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (!BCRYPT_SUCCESS(BCryptCreateHash(BCRYPT_MD5_ALG_HANDLE, &hash, nullptr, 0, nullptr, 0, 0))) return std::nullopt;

    const bool read = readChunks(file, [hash](char* data, std::size_t size) {
        return BCRYPT_SUCCESS(BCryptHashData(hash, reinterpret_cast<PUCHAR>(data), static_cast<ULONG>(size), 0));
    });

    Digest digest;
    const bool finished = read && BCRYPT_SUCCESS(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0));
    BCryptDestroyHash(hash);

    if (!finished) return std::nullopt;
    return digest;
}

#elif defined(__APPLE__)

// Apple deprecates MD5 for cryptographic uses only
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

std::optional<Digest> md5(const std::filesystem::path& file) {
    CC_MD5_CTX context;
    CC_MD5_Init(&context);

    const bool read = readChunks(file, [&context](char* data, std::size_t size) {
        CC_MD5_Update(&context, data, static_cast<CC_LONG>(size));
        return true;
    });

    Digest digest;
    CC_MD5_Final(digest.data(), &context);

    if (!read) return std::nullopt;
    return digest;
}

#pragma clang diagnostic pop

#endif

}  // namespace

std::string md5File(const std::filesystem::path& file) {
    // POSIX systems open directories as files, read as empty
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) return {};

    const auto digest = md5(file);
    if (!digest) return {};

    constexpr std::string_view digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(digest->size() * 2);
    for (const auto byte : *digest) {
        hex += digits[static_cast<unsigned>(byte) >> 4U];
        hex += digits[static_cast<unsigned>(byte) & 0x0FU];
    }
    return hex;
}

}  // namespace hash

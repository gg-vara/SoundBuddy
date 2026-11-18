// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Pkce.h"

#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecRandom.h>
#endif

namespace net::pkce {

namespace {

using Bytes = std::array<unsigned char, 32>;

// without padding, as RFC 7636 asks
std::string base64Url(std::span<const unsigned char> bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string encoded;
    unsigned bits = 0;
    int pending = 0;
    for (const auto byte : bytes) {
        bits = (bits << 8U) | byte;
        pending += 8;
        while (pending >= 6) {
            pending -= 6;
            encoded += alphabet[(bits >> static_cast<unsigned>(pending)) & 0x3FU];
        }
    }
    if (pending > 0) encoded += alphabet[(bits << static_cast<unsigned>(6 - pending)) & 0x3FU];
    return encoded;
}

#ifdef _WIN32

Bytes random() {
    Bytes bytes {};
    if (!BCRYPT_SUCCESS(BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG))) {
        throw std::runtime_error("No random bytes from the system");
    }
    return bytes;
}

Bytes sha256(std::string_view text) {
    Bytes digest {};
    const auto hashed = BCryptHash(
        BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())), static_cast<ULONG>(text.size()),
        digest.data(), static_cast<ULONG>(digest.size())
    );
    if (!BCRYPT_SUCCESS(hashed)) throw std::runtime_error("No SHA-256 from the system");
    return digest;
}

#elif defined(__APPLE__)

Bytes random() {
    Bytes bytes {};
    if (SecRandomCopyBytes(kSecRandomDefault, bytes.size(), bytes.data()) != errSecSuccess) {
        throw std::runtime_error("No random bytes from the system");
    }
    return bytes;
}

Bytes sha256(std::string_view text) {
    Bytes digest {};
    CC_SHA256(text.data(), static_cast<CC_LONG>(text.size()), digest.data());
    return digest;
}

#endif

}  // namespace

std::string randomValue() {
    return base64Url(random());
}

std::string challengeOf(std::string_view verifier) {
    return base64Url(sha256(verifier));
}

}  // namespace net::pkce

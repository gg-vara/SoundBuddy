// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifdef _WIN32

#include "SecretStore.h"

#include <windows.h>

#include <dpapi.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace secrets {

namespace {

// a blob DPAPI allocated
using Owned = std::unique_ptr<BYTE, decltype(&LocalFree)>;

DATA_BLOB blobOf(const void* data, std::size_t size) {
    // DPAPI does not write to what it is given
    return { .cbData = static_cast<DWORD>(size), .pbData = static_cast<BYTE*>(const_cast<void*>(data)) };
}

}  // namespace

bool store(const std::filesystem::path& name, std::string_view secret) {
    auto input = blobOf(secret.data(), secret.size());
    DATA_BLOB output {};
    if (CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output) == FALSE) return false;
    const Owned owned { output.pbData, &LocalFree };

    std::error_code error;
    std::filesystem::create_directories(name.parent_path(), error);
    if (error) return false;

    // written aside, then renamed over the previous one : an interrupted write never loses what was there
    auto temporary = name;
    temporary += ".tmp";

    std::ofstream stream { temporary, std::ios::binary | std::ios::trunc };
    stream.write(reinterpret_cast<const char*>(output.pbData), static_cast<std::streamsize>(output.cbData));
    stream.close();

    if (stream) std::filesystem::rename(temporary, name, error);
    if (!stream || error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

std::optional<std::string> load(const std::filesystem::path& name) {
    std::ifstream stream { name, std::ios::binary };
    if (!stream) return std::nullopt;
    const std::vector<char> content { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

    auto input = blobOf(content.data(), content.size());
    DATA_BLOB output {};
    if (CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output) == FALSE) return std::nullopt;
    const Owned owned { output.pbData, &LocalFree };

    return std::string { reinterpret_cast<const char*>(output.pbData), output.cbData };
}

void remove(const std::filesystem::path& name) {
    std::error_code error;
    std::filesystem::remove(name, error);
}

}  // namespace secrets

#endif

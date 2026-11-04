// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Platform.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace platform {

namespace {

std::optional<Isolation>& currentIsolation() {
    static std::optional<Isolation> instance;
    return instance;
}

}  // namespace

void isolate(Isolation isolation) {
    currentIsolation() = std::move(isolation);
}

const Isolation* isolation() {
    const auto& current = currentIsolation();
    return current ? &*current : nullptr;
}

bool isIsolated() {
    return isolation() != nullptr;
}

std::string toUtf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return { text.begin(), text.end() };
}

std::filesystem::path fromUtf8(std::string_view text) {
    return std::u8string { text.begin(), text.end() };
}

}  // namespace platform

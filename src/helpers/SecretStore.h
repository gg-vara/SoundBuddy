// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// What only this user of this computer may read, by the means of the system : DPAPI on Windows, the Keychain on macOS.
// Named after a path of the data folder, so that a run with --data-dir never reaches the secrets of the real one
namespace secrets {

bool store(const std::filesystem::path& name, std::string_view secret);

std::optional<std::string> load(const std::filesystem::path& name);

void remove(const std::filesystem::path& name);

}  // namespace secrets

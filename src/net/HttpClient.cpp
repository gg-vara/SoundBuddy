// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// platform independent parts of HttpClient; see HttpClient.win.cpp and HttpClient.mac.mm for the transports

#include "HttpClient.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace net {

std::optional<std::string> Response::header(std::string_view name) const {
    const auto sameName = [name](const Header& header) {
        return header.name.size() == name.size() && std::equal(name.begin(), name.end(), header.name.begin(), [](char a, char b) {
                   return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
               });
    };

    const auto found = std::ranges::find_if(headers, sameName);
    if (found == headers.end()) return std::nullopt;
    return found->value;
}

}  // namespace net

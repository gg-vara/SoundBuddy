// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

// PKCE (RFC 7636) : what proves that the one exchanging a code is the one who asked for it, as SoundBuddy cannot keep a secret
namespace net::pkce {

// unguessable, from the system's generator : 32 bytes, as the 43 characters of base64url a verifier needs. Also serves as "state"
std::string randomValue();

// what the authorization request carries : SHA-256 of the verifier, in base64url ("S256")
std::string challengeOf(std::string_view verifier);

}  // namespace net::pkce

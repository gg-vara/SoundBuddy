// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

// Translations, without any framework dependency.
// Texts are written in English in the code; translations are compiled in, from src/i18n/*.inc tables.
namespace i18n {

enum class Language {
    English,  // source language
    French
};

// language of the user interface, detected from the system settings on first use
Language language();

// overrides the detected language
void setLanguage(Language language);

// ISO 639-1 code of the current language, as sent in Accept-Language headers : "en", "fr"
std::string_view languageCode();

// size in bytes, in the current language, with 1024 multiples : "12.34 MB", "12,34 Mo"
std::string formatDataSize(std::uint64_t bytes);

// translation of an English text in the current language, or the text itself if untranslated (UTF-8)
std::string_view tr(std::string_view source);

// replaces %1, %2... placeholders by "args", like QString::arg()
std::string format(std::string_view text, std::initializer_list<std::string_view> args);

}  // namespace i18n

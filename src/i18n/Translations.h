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

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// src/i18n

#include "Check.h"
#include "src/i18n/Translations.h"

int main() {
    i18n::setLanguage(i18n::Language::French);
    CHECK(i18n::tr("Quit") == "Quitter", "known text translated");
    CHECK(i18n::tr("Logged as \"%1\"") == "Connecté en tant que \"%1\"", "text with quotes and placeholder");
    CHECK(i18n::tr("Not in the table") == "Not in the table", "unknown text falls back to English");
    CHECK(i18n::languageCode() == "fr", "French code");

    CHECK(i18n::format(i18n::tr("Logged as \"%1\""), { "guillaume" }) == "Connecté en tant que \"guillaume\"", "format %1");
    CHECK(
        i18n::format("%2 then %1, 100% sure %3", { "a", "b" }) == "b then a, 100% sure %3", "format : order, literal %, missing argument"
    );

    CHECK(i18n::formatDataSize(12) == "12 o" && i18n::formatDataSize(1536) == "1,50 Ko", "French sizes");

    i18n::setLanguage(i18n::Language::English);
    CHECK(i18n::tr("Quit") == "Quit", "English keeps source texts");
    CHECK(i18n::languageCode() == "en", "English code");
    CHECK(i18n::formatDataSize(45'678'901) == "43.56 MB" && i18n::formatDataSize(3ULL << 40U) == "3072.00 GB", "English sizes, up to GB");

    return check::summary();
}

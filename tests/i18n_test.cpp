// src/i18n, and the Qt translator bridge

#include <QCoreApplication>

#include "Check.h"
#include "src/i18n/QtTranslatorBridge.hpp"
#include "src/i18n/Translations.h"

int main(int argc, char** argv) {
    i18n::setLanguage(i18n::Language::French);
    CHECK(i18n::tr("Quit") == "Quitter", "known text translated");
    CHECK(i18n::tr("Logged as \"%1\"") == "Connecté en tant que \"%1\"", "text with quotes and placeholder");
    CHECK(i18n::tr("Not in the table") == "Not in the table", "unknown text falls back to English");
    CHECK(i18n::languageCode() == "fr", "French code");

    CHECK(i18n::format(i18n::tr("Logged as \"%1\""), { "guillaume" }) == "Connecté en tant que \"guillaume\"", "format %1");
    CHECK(i18n::format("%2 then %1, 100% sure %3", { "a", "b" }) == "b then a, 100% sure %3", "format : order, literal %, missing argument");

    CHECK(i18n::formatDataSize(12) == "12 o" && i18n::formatDataSize(1536) == "1,50 Ko", "French sizes");

    i18n::setLanguage(i18n::Language::English);
    CHECK(i18n::tr("Quit") == "Quit", "English keeps source texts");
    CHECK(i18n::languageCode() == "en", "English code");
    CHECK(i18n::formatDataSize(45'678'901) == "43.56 MB" && i18n::formatDataSize(3ull << 40) == "3072.00 GB", "English sizes, up to GB");

    // Qt bridge
    QCoreApplication app(argc, argv);
    app.installTranslator(new QtTranslatorBridge(&app));

    i18n::setLanguage(i18n::Language::French);
    CHECK(QCoreApplication::translate("MainWindow", "Quit") == QString::fromUtf8("Quitter"), "Qt tr() served by the bridge");
    CHECK(QCoreApplication::translate("MainWindow", "Unknown text") == "Unknown text", "Qt falls back to the source text");

    i18n::setLanguage(i18n::Language::English);
    CHECK(QCoreApplication::translate("MainWindow", "Quit") == "Quit", "Qt tr() in English");

    return check::summary();
}

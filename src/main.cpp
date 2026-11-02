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

#include <QString>
#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QDebug>

#include <Velopack.hpp>

#include <cstdlib>
#include <exception>
#include <string_view>

#include "helpers/Platform.h"
#include "i18n/QtTranslatorBridge.hpp"
#include "logs/Logs.h"
#include "ui/monitor/mainWindow.h"

namespace {

QString getResourcesPath() {
#ifdef Q_OS_OSX
    return QApplication::applicationDirPath() + QDir::separator() + ".." + QDir::separator() + "Resources" + QDir::separator();
#else
    return QApplication::applicationDirPath() + QDir::separator();
#endif
}

logs::Level toLevel(std::string_view velopackLevel) {
    if (velopackLevel.starts_with("err") || velopackLevel.starts_with("Err")) return logs::Level::Error;
    if (velopackLevel.starts_with("warn") || velopackLevel.starts_with("Warn")) return logs::Level::Warning;
    if (velopackLevel.starts_with("info") || velopackLevel.starts_with("Info")) return logs::Level::Info;
    return logs::Level::Debug;
}

// installer and updates
void logVelopackMessage(void* /*userData*/, const char* level, const char* message) {
    logs::write(toLevel(level ? level : ""), "Velopack", message ? message : "");
}

// Qt own messages, until the app no longer relies on Qt
logs::Level toLevel(QtMsgType type) {
    switch (type) {
        case QtDebugMsg:
            return logs::Level::Debug;
        case QtInfoMsg:
            return logs::Level::Info;
        case QtWarningMsg:
            return logs::Level::Warning;
        case QtCriticalMsg:
        case QtFatalMsg:
            break;
    }
    return logs::Level::Error;
}

void logQtMessage(QtMsgType type, const QMessageLogContext& /*context*/, const QString& message) {
    logs::write(toLevel(type), "Qt", message.toStdString());
}

// last words before quitting on an unhandled exception : never throws
void logFatalError(const char* what) noexcept {
    try {
        logs::error("App", "Fatal error : {}", what);
        logs::stop();
    } catch (...) {  // NOLINT(bugprone-empty-catch) : nothing left to report to
    }
}

int run(int argc, char** argv) {
    logs::start(platform::dataDirectory() / "logs" / "soundbuddy.log");

    // must run first : handles install / update / uninstall hooks, and may exit the process right away
    Velopack::VelopackApp::Build().SetLogger(&logVelopackMessage, nullptr).Run();

    logs::info("App", "{} {} started", APP_NAME, APP_CURRENT_VERSION);
    qInstallMessageHandler(&logQtMessage);

    // prevent multiples instances
    if (!platform::claimSingleInstance()) {
        logs::info("App", "Already running, exiting");
        return 1;
    }

    // setup app
    const QApplication app(argc, argv);

    // activate translations, in the user interface language of the system...
    const auto locale = i18n::language() == i18n::Language::French ? QLocale(QLocale::French) : QLocale(QLocale::English);

    // Qt own texts (standard buttons...)
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, "qt", "_", getResourcesPath() + "translations")) {
        QApplication::installTranslator(&qtTranslator);
    }

    // app
    QtTranslatorBridge appTranslator;
    QApplication::installTranslator(&appTranslator);

    //
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setApplicationName(APP_NAME);
    QApplication::setOrganizationName(APP_PUBLISHER_ORG);
    QApplication::setApplicationDisplayName(APP_NAME);

    // fetch main window
    const MainWindow mw;

    // wait for the app to close
    const auto exitCode = QApplication::exec();
    logs::info("App", "Exiting with code {}", exitCode);
    logs::stop();
    return exitCode;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& e) {
        logFatalError(e.what());
    } catch (...) {
        logFatalError("unknown exception");
    }
    return EXIT_FAILURE;
}

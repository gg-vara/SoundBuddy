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

// Qt user interface, on macOS until it moves to the system webview as on Windows

#include <QString>
#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QDebug>

#include "src/ui/Run.h"
#include "src/logs/Logs.h"
#include "QtTranslatorBridge.hpp"
#include "monitor/mainWindow.h"

namespace {

QString getResourcesPath() {
#ifdef Q_OS_OSX
    return QApplication::applicationDirPath() + QDir::separator() + ".." + QDir::separator() + "Resources" + QDir::separator();
#else
    return QApplication::applicationDirPath() + QDir::separator();
#endif
}

// Qt own messages
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

}  // namespace

int ui::run(int argc, char** argv) {
    qInstallMessageHandler(&logQtMessage);

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
    return QApplication::exec();
}

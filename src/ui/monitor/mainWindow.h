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

#include <QCoreApplication>
#include <QString>
#include <QSize>
#include <QObject>
#include <QDebug>
#include <QTabWidget>
#include <QMainWindow>
#include <QWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHideEvent>
#include <QIcon>
#include <QStatusBar>

#include <memory>

#include "src/helpers/Platform.h"

#include "src/workers/feeder/FeederThread.h"
#include "src/workers/shout/ShoutUploader.h"
#include "src/workers/shout/ShoutWatcher.h"
#include "src/ui/tabs/ShoutTab.hpp"
#include "src/ui/tabs/FeederTab.hpp"
#include "src/workers/mBeat/MBeatThread.h"
#include "src/workers/base/UpdateChecker.h"

#include "src/ui/widgets/TraficLight.hpp"

#include "src/helpers/AppSettings.h"
#include "src/ui/monitor/PreferencesDialog.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

 public:
    MainWindow();
    void informWarningPresence();

 protected:
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

 private:
    bool _forceQuitOnMacOs = false;
    bool _userNotificationOnUpdateCheck = false;
    QSystemTrayIcon* _trayIcon = nullptr;
    QAction* _myPlatformAction = nullptr;
    QAction* _openWarningsAction = nullptr;
    AppSettings _appSettings;

    // statusbar
    void _initStatusBar();
    void _updateStatusBar(const QString& message, const ConnectivityIndicator& indic);
    QLabel* _statusLabel = nullptr;
    TrafficLightWidget* _statusLight = nullptr;

    ///
    /// UI instantiation
    ///

    void _initUI();
    void _initUIMenu();
    void _initUITray();

    QMenu* _getFileMenu(bool withMonitor);
    QMenu* _getOptionsMenu();

    void _onAppSettingsChanged();
    void _updateWarningsMenuItem();

    ///
    /// Functionnalities helpers calls
    ///

    void _accessPlatform();
    void _accessPreferences();
    void _openWarnings();

    ///
    /// Events handling
    ///

    // visibility
    void _trueShow();
    void _trueHide(QEvent* event);
    void _forcedClose();
    void _iconActivated(QSystemTrayIcon::ActivationReason reason);

    // update handling
    QAction* _versionAction = nullptr;
    QAction* _cfugAction = nullptr;
    void _setupAutoUpdate();
    void _onUpdateChecked(const UpdateChecker::CheckResults& checkResults);
    void _requireUpdateCheckFromUser();
    void _checkForAppUpdates();
    void _updateSearchSwitchUi(bool isSearching);

    // tabs
    ShoutTab* _shoutTab = nullptr;
    FeederTab* _feederTab = nullptr;
    void _initUITabs();

    // workers, stopped on destruction (in reverse order) : the watcher, which hands shouts to the uploader, stops first
    std::unique_ptr<ShoutUploader> _shoutUploader;
    std::unique_ptr<ShoutWatcher> _shoutWorker;
    std::unique_ptr<FeederThread> _feederWorker;
    std::unique_ptr<MBeatThread> _mbeatWorker;
    UpdateChecker _updateChecker;

    void _runMBeat();
    void _runShouts();
    void _runFeeder();
};

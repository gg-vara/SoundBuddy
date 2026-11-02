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

#include "mainWindow.h"
#include "version.h"

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "src/helpers/Json.hpp"
#include "src/ui/WorkerBridge.hpp"

MainWindow::MainWindow() {
    // generate the UI
    this->_initUI();

    //
    this->_onAppSettingsChanged();
    this->_updateWarningsMenuItem();
    this->_setupAutoUpdate();

    // autostart shout thread ?
    if (this->_appSettings.values().autoRunShout) {
        this->_shoutTab->button()->click();
    }
}

void MainWindow::informWarningPresence() {
    this->_updateWarningsMenuItem();
}

// updates the menu depending on the config values filled or not
void MainWindow::_onAppSettingsChanged() {
    //
    bool shouldActivateLink = !this->_appSettings.getConnectivityInfos().getPlaformHomeUrl().empty();
    this->_myPlatformAction->setEnabled(shouldActivateLink);

    // restart threads ?
    _runMBeat();
    if(this->_shoutWorker && this->_shoutWorker->isRunning()) {
        _runShouts();
    }
    if(this->_feederWorker && this->_feederWorker->isRunning()) {
        _runFeeder();
    }
}

void MainWindow::_updateWarningsMenuItem() {
    std::error_code error;
    bool hasFeederWarnings = std::filesystem::exists(AppSettings::getFeedWarningFilePath(), error);
    this->_openWarningsAction->setEnabled(hasFeederWarnings);
}

///
/// Functionnalities helpers calls
///

void MainWindow::_accessPlatform() {
    const auto platformHomeUrl = this->_appSettings.getConnectivityInfos().getPlaformHomeUrl();

    //
    if(platformHomeUrl.empty()) {
        QMessageBox::critical(this,
            tr("Cannot access %1 plaform").arg(DEST_PLATFORM_PRODUCT_NAME),
            tr("%1 Plaform URL is not valid. Please check Preferences connectivity parameters.").arg(DEST_PLATFORM_PRODUCT_NAME)
        );
        return;
    }

    //
    platform::openUrl(platformHomeUrl);
}

void MainWindow::_accessPreferences() {
    PreferencesDialog(&this->_appSettings, this).exec();
    this->_onAppSettingsChanged();
}

// open the warnings file on the OS
void MainWindow::_openWarnings() {
    //
    std::ifstream warnings { AppSettings::getFeedWarningFilePath(), std::ios::binary };
    const std::string content { std::istreambuf_iterator<char>(warnings), std::istreambuf_iterator<char>() };

    // array of tracks, each an object of string values
    const auto doc = json::parse(content);
    const auto rows = json::root(doc);

    //
    auto howMany = static_cast<int>(yyjson_arr_size(rows));
    if(howMany == 0) {
        QMessageBox::information(this, tr("No warnings found"), tr("Congratulations, your latest export was flawless !"));
        return;
    }

    QDialog display(this);
    display.setModal(true);
    display.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // columns : the keys of the first track
    std::vector<std::string_view> columns;
    QStringList headers;
    {
        std::size_t index = 0;
        std::size_t count = 0;
        yyjson_val* key = nullptr;
        yyjson_val* value = nullptr;
        yyjson_obj_foreach(yyjson_arr_get_first(rows), index, count, key, value) {
            columns.push_back(json::string(key));
            headers << QString::fromUtf8(columns.back().data(), static_cast<qsizetype>(columns.back().size()));
        }
    }

    QTableWidget table(howMany, static_cast<int>(columns.size()), &display);
    table.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    table.horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table.setEditTriggers(QAbstractItemView::NoEditTriggers);
    table.setHorizontalHeaderLabels(headers);

    // for each row
    howMany = 0;
    std::size_t rowIndex = 0;
    std::size_t rowCount = 0;
    yyjson_val* cellsData = nullptr;
    yyjson_arr_foreach(rows, rowIndex, rowCount, cellsData) {
        // each cell
        int col = 0;
        for (const auto key : columns) {
            const auto cell = json::string(yyjson_obj_getn(cellsData, key.data(), key.size()));
            const auto cellData = QString::fromUtf8(cell.data(), static_cast<qsizetype>(cell.size()));
            if(cellData.isEmpty()) {
                auto item = new QTableWidgetItem(this->style()->standardIcon(QStyle::SP_MessageBoxWarning), tr("Missing !"));
                auto font = item->font();
                font.setBold(true);
                font.setUnderline(true);
                item->setFont(font);
                table.setItem(howMany, col, item);
            } else {
                table.setItem(howMany, col, new QTableWidgetItem(cellData));
            }
            col++;
        }

        howMany++;
    }

    //
    QVBoxLayout layout;
    layout.addWidget(&table);
    display.setLayout(&layout);


    // Get the screen size (Qt5)
    QRect screenGeometry = this->screen()->geometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();

    // Set the dialog size to be at least half the size of the screen
    display.resize(screenWidth / 2, screenHeight / 2);
    display.exec();
}

void MainWindow::_setupAutoUpdate() {
    // start the update check
    this->_checkForAppUpdates();
}

void MainWindow::_onUpdateChecked(const UpdateChecker::CheckResults &checkResults) {
    // if the user asks directly to check updates
    const bool userAsked = this->_userNotificationOnUpdateCheck;

    // next check will go un-notified
    this->_userNotificationOnUpdateCheck = false;

    this->_updateSearchSwitchUi(/*isSearching=*/false);

    switch (checkResults.result) {
        case UpdateChecker::Result::UpdateReady:
            break;

        case UpdateChecker::Result::NoUpdate:
            if (userAsked) {
                QMessageBox::information(this,
                    tr("Checking updates"),
                    tr("No updates available at the time."),
                    QMessageBox::Ok,
                    QMessageBox::Ok);
            }
            return;

        case UpdateChecker::Result::NotInstalled:
            if (userAsked) {
                QMessageBox::information(this,
                    tr("Checking updates"),
                    tr("Updates are only available for installed versions of %1.").arg(APP_NAME),
                    QMessageBox::Ok,
                    QMessageBox::Ok);
            }
            return;

        case UpdateChecker::Result::Failed:
            if (userAsked) {
                QMessageBox::critical(this,
                    tr("Error while checking updates"),
                    QString::fromStdString(checkResults.error),
                    QMessageBox::Ok,
                    QMessageBox::Ok);
            }
            return;
    }

    // update downloaded : if declined, it is applied on next launch anyway
    auto msgboxRslt = QMessageBox::information(this,
        tr("Update Available"),
        tr("%1 %2 is ready to be installed. Would you like to restart %1 now to install it ?")
            .arg(APP_NAME)
            .arg(QString::fromStdString(checkResults.version)),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (msgboxRslt == QMessageBox::Yes) {
        this->_updateChecker.applyUpdateOnExit();
        this->_forcedClose();
    }
}

void MainWindow::_requireUpdateCheckFromUser() {
    this->_userNotificationOnUpdateCheck = true;
    this->_checkForAppUpdates();
}

void MainWindow::_checkForAppUpdates() {
    this->_updateSearchSwitchUi(/*isSearching=*/true);
    // a check already in progress will end the search the same way
    this->_updateChecker.check([this](const UpdateChecker::CheckResults &results) {
        ui::post(this, [this, results = results] { this->_onUpdateChecked(results); });
    });
}

void MainWindow::_updateStatusBar(const QString &message, const ConnectivityIndicator &indic) {
    this->_statusLabel->setText(message);
    this->_statusLight->setCurrentIndex(static_cast<int>(indic));
}

void MainWindow::_runMBeat() {
    // the previous heartbeat stops first
    this->_mbeatWorker.reset();

    this->_mbeatWorker = std::make_unique<MBeatThread>(this->_appSettings.getConnectivityInfos(), [this](const mbeat::Status &status) {
        const auto indicator =
            status.connectivity == mbeat::Connectivity::Connected ? ConnectivityIndicator::OK :
            status.connectivity == mbeat::Connectivity::Checking ? ConnectivityIndicator::ONGOING :
            ConnectivityIndicator::NOK;

        ui::post(this, [this, text = QString::fromStdString(status.text), indicator] {
            this->_updateStatusBar(text, indicator);
        });
    });
}

void MainWindow::_runShouts() {
    // the watcher stops first, as it feeds the uploader
    this->_shoutWorker.reset();

    if (this->_shoutUploader) {
        this->_shoutUploader->waitForDrain(std::chrono::seconds { 1 });
        this->_shoutUploader.reset();
    }

    const auto tab = this->_shoutTab;
    const auto run = tab->workerStarted();

    this->_shoutUploader = std::make_unique<ShoutUploader>(this->_appSettings.getConnectivityInfos(), tab->messageSink());

    // the watcher always stops before the uploader is destroyed : it hands shouts over directly
    this->_shoutWorker = std::make_unique<ShoutWatcher>(ShoutWatcher::Callbacks {
        .onMessage = tab->messageSink(),
        .onShout = [uploader = this->_shoutUploader.get()](std::string shoutAsJson) { uploader->upload(std::move(shoutAsJson)); },
        .onFinished = [tab, run] { ui::post(tab, [tab, run] { tab->workerFinished(run); }); },
    });
}

void MainWindow::_runFeeder() {
    // the previous run stops first
    this->_feederWorker.reset();

    const auto tab = this->_feederTab;
    const auto run = tab->workerStarted();

    this->_feederWorker = std::make_unique<FeederThread>(this->_appSettings.getConnectivityInfos(), FeederThread::Callbacks {
        .onMessage = tab->messageSink(),
        .onFilesGenerated = [this] { ui::post(this, [this] { this->_updateWarningsMenuItem(); }); },
        .onFinished = [tab, run] { ui::post(tab, [tab, run] { tab->workerFinished(run); }); },
    });
}

//
// UI
//

// main initialization
void MainWindow::_initUI() {
    // values specific to this
    #ifdef _DEBUG
        QString stdTitle = (QString)"DEBUG - " + APP_NAME;
    #else
        QString stdTitle = APP_NAME;
    #endif
    
    this->setWindowTitle(stdTitle);
    this->setMinimumSize(QSize(480, 400));
    this->setWindowIcon(QIcon(":/icons/app.png"));

    // helpers
    this->_initUITabs();
    this->_initUITray();
    this->_initUIMenu();
    this->_initStatusBar();

    #ifdef _DEBUG
        this->_trueShow();
    #endif
}


//////////
// Tray //
//////////

void MainWindow::_initUITray() {
    auto trayIcon = new QSystemTrayIcon;
    this->_trayIcon = trayIcon;
    trayIcon->setIcon(QIcon(":/icons/app_reverse.png"));
    trayIcon->setToolTip(this->windowTitle());

    QObject::connect(
        this->_trayIcon, &QSystemTrayIcon::activated,
        this, &MainWindow::_iconActivated
    );

    QMenu* cMenu = nullptr;

    #ifdef _WIN32
        cMenu = this->_getFileMenu(/*withMonitor=*/true);
    #endif

    #ifdef __APPLE__
        cMenu = new QMenu("");
        cMenu->addMenu(this->_getFileMenu(/*withMonitor=*/true));
        cMenu->addMenu(this->_getOptionsMenu());
    #endif

    this->_trayIcon->setContextMenu(cMenu);

    trayIcon->show();
}

void MainWindow::_initStatusBar() {
    //
    auto statusBar = new QStatusBar;

    auto sbWidget = new QWidget;
    this->_statusLight = new TrafficLightWidget;
    this->_statusLabel = new QLabel;

    // add connectivity infos to bar
    sbWidget->setLayout(new QHBoxLayout);
    sbWidget->layout()->addWidget(this->_statusLight);
    sbWidget->layout()->addWidget(this->_statusLabel);
    statusBar->addWidget(sbWidget);

    //
    this->setStatusBar(statusBar);
}

void MainWindow::_initUITabs() {
    auto tabs = new QTabWidget(this);
    this->_shoutTab = new ShoutTab(this, &this->_appSettings);
    this->_feederTab = new FeederTab(this);

    QObject::connect(
        this->_shoutTab->button(), &QPushButton::clicked,
        this, &MainWindow::_runShouts
    );

    QObject::connect(
        this->_feederTab->button(), &QPushButton::clicked,
        this, &MainWindow::_runFeeder
    );

    tabs->addTab(this->_shoutTab, "Shout!");
    tabs->addTab(this->_feederTab, "Feeder");
    this->setCentralWidget(tabs);
}

void MainWindow::_initUIMenu() {
    auto menuBar = new QMenuBar;
    menuBar->addMenu(this->_getFileMenu(/*withMonitor=*/false));
    menuBar->addMenu(this->_getOptionsMenu());
    this->setMenuWidget(menuBar);
}

//////////////
// End Tray //
//////////////

//////////////////////
/// Menu components //
//////////////////////

QMenu* MainWindow::_getOptionsMenu() {
    QMenu *optionsMenuItem = new QMenu(tr("Options"));

    // for checking available updates
    this->_cfugAction = new QAction(tr("Check for updates"), optionsMenuItem);
        QObject::connect(
        this->_cfugAction, &QAction::triggered,
        this, &MainWindow::_requireUpdateCheckFromUser
    );

    this->_versionAction = new QAction(APP_FULL_DENOM, optionsMenuItem);
    this->_versionAction->setEnabled(false);

    optionsMenuItem->addAction(this->_cfugAction);
    optionsMenuItem->addSeparator();
    optionsMenuItem->addAction(this->_versionAction);

    return optionsMenuItem;
}

void MainWindow::_updateSearchSwitchUi(bool isSearching) {
    //
    this->_cfugAction->setEnabled(!isSearching);

    //
    QString descr = APP_FULL_DENOM;
    if (isSearching) descr += " - " + tr("Searching for updates...");

    //
    this->_versionAction->setText(descr);
}

QMenu* MainWindow::_getFileMenu(bool withMonitor) {
    //
    auto fileMenuItem = new QMenu(tr("File"));

    // myPlatformAction
    this->_myPlatformAction = new QAction(tr("My %1").arg(DEST_PLATFORM_PRODUCT_NAME), fileMenuItem);
    _myPlatformAction->setEnabled(false);
    QObject::connect(
        _myPlatformAction, &QAction::triggered,
        this, &MainWindow::_accessPlatform
    );

    // accessPreferencesAction
    auto accessPreferencesAction = new QAction(tr("Preferences"), fileMenuItem);
    QObject::connect(
        accessPreferencesAction, &QAction::triggered,
        this, &MainWindow::_accessPreferences
    );

    // openWarningsAction
    this->_openWarningsAction = new QAction(tr("Read latest upload warnings report"), fileMenuItem);
    this->_openWarningsAction->setEnabled(false);
    QObject::connect(
        this->_openWarningsAction, &QAction::triggered,
        this, &MainWindow::_openWarnings
    );

    // openData
    auto openDataFolder = new QAction(tr("Access upload data folder"), fileMenuItem);
    QObject::connect(openDataFolder, &QAction::triggered, [=]() {
        platform::openPath(platform::dataDirectory());
    });

    // quit
    auto quitAction = new QAction(tr("Quit"), fileMenuItem);
    QObject::connect(
        quitAction, &QAction::triggered,
        this, &MainWindow::_forcedClose
    );

    if(withMonitor) {
        // monitorAction
        auto monitorAction = new QAction(tr("Open monitor..."), fileMenuItem);
        QObject::connect(
            monitorAction, &QAction::triggered,
            this, &MainWindow::_trueShow
        );

        fileMenuItem->addAction(monitorAction);
        fileMenuItem->addSeparator();
    }

    fileMenuItem->addAction(accessPreferencesAction);
    fileMenuItem->addAction(_myPlatformAction);
    fileMenuItem->addSeparator();
    fileMenuItem->addAction(_openWarningsAction);
    fileMenuItem->addAction(openDataFolder);
    fileMenuItem->addSeparator();
    fileMenuItem->addAction(quitAction);

    return fileMenuItem;
}

//////////////////////////
/// End Menu components //
//////////////////////////

//////////////////////
/// Events handling //
//////////////////////

void MainWindow::closeEvent(QCloseEvent *event) {
    // apple specific behaviour, prevent closing
    #ifdef __APPLE__
        if (!this->_forceQuitOnMacOS) {
            return this->_trueHide(event);
        }
    #endif

    // if running shout thread
    if (this->_shoutWorker && this->_shoutWorker->isRunning()) {
        auto msgboxRslt = QMessageBox::warning(this,
            tr("Shout worker running !"),
            tr("Shout worker is actually running : Are you sure you want to exit ?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (msgboxRslt == QMessageBox::Yes) {
            // makes sure to wait for ShoutWatcher to end. Limits COM app retention on Windows
            this->_shoutWorker.reset();

            // the last shout ("nothing playing") gets a chance to be uploaded
            this->_shoutUploader->waitForDrain(std::chrono::seconds { 1 });
        } else {
            event->ignore();
            return;
        }
    }

    // stops the heartbeat, aborting its request in progress if any
    this->_mbeatWorker.reset();

    // hide trayicon on shutdown for Windows, refereshes the UI frames of system tray
    this->_trayIcon->hide();
    QApplication::quit();
}

void MainWindow::_trueShow() {
    this->showNormal();
    this->activateWindow();
    this->raise();
}

// hide window on minimize, only triggered on windows
void MainWindow::hideEvent(QHideEvent *event) {
    this->_trueHide(event);
}

void MainWindow::_trueHide(QEvent* event) {
    event->ignore();
    this->hide();
}

void MainWindow::_forcedClose() {
    this->_forceQuitOnMacOs = true;
    this->close();
}

// tray click handling
void MainWindow::_iconActivated(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
    case QSystemTrayIcon::DoubleClick:
        this->_trueShow();
        break;
    default:
        break;
    }
}

//////////////////////////
/// End Events handling //
//////////////////////////

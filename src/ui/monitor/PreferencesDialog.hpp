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

#include <QDialog>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QCheckBox>

#include "src/helpers/AppSettings.hpp"
#include "version.h"

class PreferencesDialog : public QDialog {
    Q_OBJECT

 public:
    explicit PreferencesDialog(AppSettings* appSettings, QWidget* parent = nullptr) : QDialog(parent), _appSettings(appSettings) {
        this->setWindowTitle(tr("Preferences"));
        this->setModal(true);
        auto layout = new QGridLayout;
        this->setLayout(layout);

        //
        // Platform connectivity
        //

        auto connectivityGroup = new QGroupBox(tr("%1 connectivity").arg(DEST_PLATFORM_PRODUCT_NAME), this);
        connectivityGroup->setLayout(new QVBoxLayout);

            this->_PlatformURLEdit = new QLineEdit(this);
            this->_PlatformUsernameEdit = new QLineEdit(this);
            this->_PlatformPasswordEdit = new QLineEdit(this);
            this->_PlatformPasswordEdit->setEchoMode(QLineEdit::Password);

            this->_PlatformURLEdit->setPlaceholderText(tr("Host URL"));
            this->_PlatformUsernameEdit->setPlaceholderText(tr("Username"));
            this->_PlatformPasswordEdit->setPlaceholderText(tr("Password"));

            this->_PlatformURLEdit->setText(this->_appSettings->value(AppSettings::PLATFORM_HOST_URL).toString());
            this->_PlatformUsernameEdit->setText(this->_appSettings->value(AppSettings::PLATFORM_USERNAME).toString());
            this->_PlatformPasswordEdit->setText(this->_appSettings->value(AppSettings::PLATFORM_PASSWORD).toString());

            connectivityGroup->layout()->addWidget(this->_PlatformURLEdit);
            connectivityGroup->layout()->addWidget(this->_PlatformUsernameEdit);
            connectivityGroup->layout()->addWidget(this->_PlatformPasswordEdit);

        layout->addWidget(connectivityGroup);

        //
        // Automation
        //

        auto automationGroup = new QGroupBox(tr("Automation"), this);
        automationGroup->setLayout(new QVBoxLayout);

            this->_launchAppAtStartupChk = new QCheckBox(tr("Launch %1 at system boot").arg(APP_NAME), this);

            this->_launchAppAtStartupChk->setChecked(this->_appSettings->value(AppSettings::MUST_RUN_AT_STARTUP).toBool());

            automationGroup->layout()->addWidget(this->_launchAppAtStartupChk);

            if (PlatformHelper::isLaunchingAtStartup()) {
                this->_launchAppAtStartupChk->setChecked(true);
            }

            QObject::connect(
                this->_launchAppAtStartupChk, &QCheckBox::checkStateChanged,
                this, &PreferencesDialog::_mightAddAppToStartup
            );

        layout->addWidget(automationGroup);

    }

 private:
    AppSettings* _appSettings;

    QLineEdit* _PlatformURLEdit;
    QLineEdit* _PlatformUsernameEdit;
    QLineEdit* _PlatformPasswordEdit;

    QCheckBox* _launchAppAtStartupChk;

    void _mightAddAppToStartup(Qt::CheckState checkState) {
        PlatformHelper::switchStartupLaunch();
        this->_appSettings->setValue(AppSettings::MUST_RUN_AT_STARTUP, Qt::CheckState::Checked == checkState);
    }

    void closeEvent(QCloseEvent *event) override {
        //
        this->_appSettings->setValue(AppSettings::PLATFORM_HOST_URL, this->_PlatformURLEdit->text());
        this->_appSettings->setValue(AppSettings::PLATFORM_USERNAME, this->_PlatformUsernameEdit->text());
        this->_appSettings->setValue(AppSettings::PLATFORM_PASSWORD, this->_PlatformPasswordEdit->text());

        //
        this->_appSettings->sync();
    }
};

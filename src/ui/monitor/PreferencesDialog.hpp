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

#include "src/helpers/AppSettings.h"
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

            this->_platformUrlEdit = new QLineEdit(this);
            this->_platformUsernameEdit = new QLineEdit(this);
            this->_platformPasswordEdit = new QLineEdit(this);
            this->_platformPasswordEdit->setEchoMode(QLineEdit::Password);

            this->_platformUrlEdit->setPlaceholderText(tr("Host URL"));
            this->_platformUsernameEdit->setPlaceholderText(tr("Username"));
            this->_platformPasswordEdit->setPlaceholderText(tr("Password"));

            const auto& values = this->_appSettings->values();
            this->_platformUrlEdit->setText(QString::fromStdString(values.platformHostUrl));
            this->_platformUsernameEdit->setText(QString::fromStdString(values.platformUsername));
            this->_platformPasswordEdit->setText(QString::fromStdString(values.platformPassword));

            connectivityGroup->layout()->addWidget(this->_platformUrlEdit);
            connectivityGroup->layout()->addWidget(this->_platformUsernameEdit);
            connectivityGroup->layout()->addWidget(this->_platformPasswordEdit);

        layout->addWidget(connectivityGroup);

        //
        // Automation
        //

        auto automationGroup = new QGroupBox(tr("Automation"), this);
        automationGroup->setLayout(new QVBoxLayout);

            this->_launchAppAtStartupChk = new QCheckBox(tr("Launch %1 at system boot").arg(APP_NAME), this);

            this->_launchAppAtStartupChk->setChecked(values.runAtStartup);

            automationGroup->layout()->addWidget(this->_launchAppAtStartupChk);

            if (platform::isLaunchingAtStartup()) {
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

    QLineEdit* _platformUrlEdit;
    QLineEdit* _platformUsernameEdit;
    QLineEdit* _platformPasswordEdit;

    QCheckBox* _launchAppAtStartupChk;

    void _mightAddAppToStartup(Qt::CheckState checkState) {
        platform::setLaunchingAtStartup(Qt::CheckState::Checked == checkState);
        this->_appSettings->values().runAtStartup = Qt::CheckState::Checked == checkState;
        this->_appSettings->save();
    }

    void closeEvent(QCloseEvent *event) override {
        //
        auto& values = this->_appSettings->values();
        values.platformHostUrl = this->_platformUrlEdit->text().toStdString();
        values.platformUsername = this->_platformUsernameEdit->text().toStdString();
        values.platformPassword = this->_platformPasswordEdit->text().toStdString();

        //
        this->_appSettings->save();
    }
};

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

#include <QThread>
#include <QString>

#include <exception>
#include <memory>
#include <optional>

#include <Velopack.hpp>

#include "src/logs/Logs.h"
#include "version.h"

// Checks GitHub releases for a newer version, and downloads it, through Velopack
class UpdateCheckerThread : public QThread {
    Q_OBJECT

 public:
    enum class Result {
        UpdateReady,   // downloaded, ready to be applied
        NoUpdate,
        NotInstalled,  // not running from a Velopack installation (development builds...)
        Failed
    };

    struct CheckResults {
        Result result = Result::Failed;
        QString version;  // of the downloaded update
        QString error;
    };

    void run() override {
        CheckResults results;

        _update.reset();
        _manager.reset();

        try {
            _manager = std::make_unique<Velopack::UpdateManager>(
                std::make_unique<Velopack::GithubSource>(APP_UPDATES_REPOSITORY_URL)
            );

            _update = _manager->CheckForUpdates();
            if (_update) {
                _manager->DownloadUpdates(*_update);
                results = { Result::UpdateReady, QString::fromStdString(_update->TargetFullRelease.Version) };
            } else {
                results.result = Result::NoUpdate;
            }
        } catch (const std::exception& e) {
            // the manager cannot be created outside of an installation
            results.result = _manager ? Result::Failed : Result::NotInstalled;
            results.error = QString::fromUtf8(e.what());
            _update.reset();
        }

        switch (results.result) {
            case Result::UpdateReady: logs::info("Updates", "Version {} downloaded", results.version.toStdString()); break;
            case Result::NoUpdate: logs::info("Updates", "Up to date"); break;
            case Result::NotInstalled: logs::info("Updates", "Not checked, not running from an installation"); break;
            case Result::Failed: logs::warning("Updates", "Check failed : {}", results.error.toStdString()); break;
        }

        emit checked(results);
    }

    // once an update is ready : the application must exit right after, Velopack then applies it and restarts
    void applyUpdateOnExit() {
        if (_manager && _update) {
            _manager->WaitExitThenApplyUpdates(*_update);
        }
    }

 signals:
    void checked(const UpdateCheckerThread::CheckResults results);

 private:
    std::unique_ptr<Velopack::UpdateManager> _manager;
    std::optional<Velopack::UpdateInfo> _update;
};

Q_DECLARE_METATYPE(UpdateCheckerThread::CheckResults)

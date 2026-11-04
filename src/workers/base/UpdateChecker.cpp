// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateChecker.h"

#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

#include <Velopack.hpp>

#include "src/helpers/Platform.h"
#include "src/logs/Logs.h"
#include "version.h"

struct UpdateChecker::State {
    std::mutex mutex;
    bool checking = false;
    bool abandoned = false;  // the checker is destroyed

    std::unique_ptr<Velopack::UpdateManager> manager;
    std::optional<Velopack::UpdateInfo> update;
};

namespace {

void logResults(const UpdateChecker::CheckResults& results) {
    using Result = UpdateChecker::Result;
    switch (results.result) {
        case Result::UpdateReady:
            logs::info("Updates", "Version {} downloaded", results.version);
            break;
        case Result::NoUpdate:
            logs::info("Updates", "Up to date");
            break;
        case Result::NotInstalled:
            logs::info("Updates", "Not checked, not running from an installation");
            break;
        case Result::Failed:
            logs::warning("Updates", "Check failed : {}", results.error);
            break;
    }
}

}  // namespace

UpdateChecker::UpdateChecker() :
    _state(std::make_shared<State>()) {}

UpdateChecker::~UpdateChecker() {
    const std::scoped_lock lock { _state->mutex };
    _state->abandoned = true;
}

bool UpdateChecker::check(OnChecked onChecked) {
    {
        const std::scoped_lock lock { _state->mutex };
        if (_state->checking) return false;
        _state->checking = true;
    }

    // detached : a download cannot be interrupted, and must not delay quitting
    std::thread([state = _state, onChecked = std::move(onChecked)] {
        CheckResults results;
        std::unique_ptr<Velopack::UpdateManager> manager;
        std::optional<Velopack::UpdateInfo> update;

        try {
            if (platform::isIsolated()) throw std::runtime_error("Isolated run");

            manager = std::make_unique<Velopack::UpdateManager>(std::make_unique<Velopack::GithubSource>(APP_UPDATES_REPOSITORY_URL));

            update = manager->CheckForUpdates();
            if (update) {
                manager->DownloadUpdates(*update);
                results = { .result = Result::UpdateReady, .version = update->TargetFullRelease.Version };
            } else {
                results.result = Result::NoUpdate;
            }
        } catch (const std::exception& e) {
            // the manager cannot be created outside of an installation
            results.result = manager ? Result::Failed : Result::NotInstalled;
            results.error = e.what();
            update.reset();
        }

        logResults(results);

        // under the lock : the checker cannot be destroyed meanwhile, nor its owner before the callback returns
        const std::scoped_lock lock { state->mutex };
        state->checking = false;
        state->manager = std::move(manager);
        state->update = std::move(update);

        if (!state->abandoned && onChecked) onChecked(results);
    }).detach();

    return true;
}

void UpdateChecker::applyUpdateOnExit() {
    const std::scoped_lock lock { _state->mutex };
    if (_state->manager && _state->update) {
        _state->manager->WaitExitThenApplyUpdates(*_state->update);
    }
}

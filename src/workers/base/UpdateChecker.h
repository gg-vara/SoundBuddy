// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>

// Checks GitHub releases for a newer version, and downloads it, through Velopack, on a background thread
class UpdateChecker {
 public:
    enum class Result {
        UpdateReady,  // downloaded, ready to be applied
        NoUpdate,
        NotInstalled,  // not running from a Velopack installation (development builds...)
        Failed
    };

    struct CheckResults {
        Result result = Result::Failed;
        std::string version;  // of the downloaded update
        std::string error;    // UTF-8
    };

    // called on the checker's thread
    using OnChecked = std::function<void(const CheckResults&)>;

    UpdateChecker();

    // does not wait for a check in progress (a download cannot be interrupted) : its results are dropped
    ~UpdateChecker();

    UpdateChecker(const UpdateChecker&) = delete;
    void operator=(const UpdateChecker&) = delete;

    // checks in the background, unless a check is already in progress (false then)
    bool check(OnChecked onChecked);

    // once an update is ready : the application must exit right after, Velopack then applies it and restarts
    void applyUpdateOnExit();

 private:
    // shared with the check thread, which may outlive the checker
    struct State;
    std::shared_ptr<State> _state;
};

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

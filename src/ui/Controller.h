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

#include <yyjson.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "src/helpers/AppSettings.h"
#include "src/workers/base/Message.h"
#include "src/workers/base/UpdateChecker.h"
#include "src/workers/feeder/FeederThread.h"
#include "src/workers/mBeat/MBeatThread.h"
#include "src/workers/shout/ShoutUploader.h"
#include "src/workers/shout/ShoutWatcher.h"

// User interface, independent of the platform : the page (src/ui/page) shows the state kept here,
// the native shell (window, tray or menu bar, dialogs : src/ui/win, src/ui/mac) hosts it
namespace ui {

enum class Severity { Information, Error };

enum class Answer { Yes, No };

// panels of the page, over the tabs
enum class Panel { Preferences, Warnings };

// what the native shell offers, on the user interface thread
class Shell {
 public:
    virtual ~Shell() = default;

    // "event" (a JSON object with a "type") to the page : false if no page is loaded
    virtual bool sendToPage(const std::string& event) = 0;

    virtual void showWindow() = 0;

    // modal question, "byDefault" being focused : true if the user answers yes
    virtual bool ask(std::string_view title, std::string_view question, Answer byDefault) = 0;

    // modal message
    virtual void tell(std::string_view title, std::string_view text, Severity severity) = 0;

    // ends the user interface
    virtual void quit() = 0;
};

// The application state and its workers, on the user interface thread
class Controller {
 public:
    // starts the heartbeat, the update check, and the shouts if set so
    explicit Controller(Shell& shell);

    // stops the workers
    ~Controller();

    Controller(const Controller&) = delete;
    void operator=(const Controller&) = delete;

    // page calls : "request" is the JSON array of the call's arguments, [name, argument]

    // answers right away, without side effects : returns JSON
    std::string query(std::string_view request);

    // acts : not within a page call, as it may wait for workers or open modal dialogs
    void command(std::string_view request);

    // native menus

    // shows the window, on "panel"
    void openPanel(Panel panel);

    void openPlatform();
    static void openDataFolder();
    void checkForUpdates();

    // asks first if shouts are running
    void requestQuit();

    bool canOpenPlatform() const;
    static bool hasWarnings();

 private:
    enum class Tab { Shout, Feeder };

    struct LogEntry {
        workers::Message message;
        std::int64_t time = 0;  // milliseconds since the epoch
    };

    // what a tab shows of its worker
    struct TabState {
        std::deque<LogEntry> log;
        bool running = false;
        int run = 0;  // to ignore the end of a run replaced meanwhile
    };

    Shell& _shell;
    AppSettings _appSettings;

    TabState _shoutTab;
    TabState _feederTab;
    mbeat::Status _status { .text = {}, .connectivity = mbeat::Connectivity::Checking };
    std::string _nowPlaying;  // last shout, as JSON
    std::optional<Panel> _pendingPanel;
    bool _searchingUpdates = false;
    bool _userAskedUpdates = false;
    bool _quitting = false;

    // workers, stopped by the destructor : the watcher, which hands shouts to the uploader, stops first
    std::unique_ptr<MBeatThread> _mbeatWorker;
    std::unique_ptr<FeederThread> _feederWorker;
    std::unique_ptr<ShoutUploader> _shoutUploader;
    std::unique_ptr<ShoutWatcher> _shoutWorker;
    UpdateChecker _updateChecker;

    TabState& _tab(Tab tab) { return tab == Tab::Shout ? _shoutTab : _feederTab; }
    static const char* _tabName(Tab tab) { return tab == Tab::Shout ? "shout" : "feeder"; }

    // workers
    workers::MessageSink _messageSink(Tab tab);
    void _runMBeat();
    void _runShouts();
    void _stopShouts();
    void _runFeeder();

    // settings
    void _savePreferences(yyjson_val* values);
    void _setRunAtStartup(bool enabled);
    void _setAutoRunShout(bool enabled);
    void _onConnectivityChanged();

    // updates
    void _checkForUpdates(bool userAsked);
    void _onUpdateChecked(const UpdateChecker::CheckResults& results);

    // state changes, sent to the page
    void _log(Tab tab, const workers::Message& message);
    int _runStarted(Tab tab);  // new log : returns the run number, for _runFinished()
    void _runFinished(Tab tab, int run);
    void _sendSettings();
    void _sendWarnings();
    void _sendUpdate();

    // JSON of the whole state, for a page just loaded, with the translations of "translationKeys"
    std::string _state(yyjson_val* translationKeys);
};

}  // namespace ui

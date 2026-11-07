// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <yyjson.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "src/settings/AppSettings.h"
#include "src/ui/PageMessages.h"
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

// what the tray icon tells, as the page does
struct TrayState {
    // the dot of the connection with the platform : none until known
    enum class Dot { None, Ok, Issue };

    bool shouting = false;  // the circle behind the icon
    Dot dot = Dot::None;
};

// what the native shell offers, on the user interface thread
class Shell {
 public:
    virtual ~Shell() = default;

    // "event" (a JSON object with a "type") to the page : false if no page is loaded
    virtual bool sendToPage(const std::string& event) = 0;

    virtual void showWindow() = 0;

    virtual void setTrayState(TrayState state) = 0;

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

    // name and version, and the build type when it is not a production one : "SoundBuddy 1.0.0 (Debug)"
    static std::string versionLabel();

    bool canOpenPlatform() const;
    static bool hasWarnings();

 private:
    using Tab = page::Tab;

    struct TabState {
        page::TabState shown;
        int run = 0;  // to ignore the end of a run replaced meanwhile
    };

    Shell& _shell;
    AppSettings _appSettings;

    TabState _shoutTab;
    TabState _feederTab;
    mbeat::Status _status { .text = {}, .connectivity = mbeat::Connectivity::Checking };
    std::string _nowPlaying;  // last shout, as JSON
    std::optional<Panel> _pendingPanel;
    TrayState::Dot _trayDot = TrayState::Dot::None;
    bool _searchingUpdates = false;
    std::optional<page::UpdateResult> _updateResult;
    bool _quitting = false;

    // workers, stopped by the destructor : the watcher, which hands shouts to the uploader, stops first
    std::unique_ptr<MBeatThread> _mbeatWorker;
    std::unique_ptr<FeederThread> _feederWorker;
    std::unique_ptr<ShoutUploader> _shoutUploader;
    std::unique_ptr<ShoutWatcher> _shoutWorker;
    UpdateChecker _updateChecker;

    TabState& _tab(Tab tab) { return tab == Tab::Shout ? _shoutTab : _feederTab; }

    // workers
    workers::MessageSink _messageSink(Tab tab);
    void _startSession();
    void _runMBeat();
    void _runShouts();
    void _stopShouts();
    void _runFeeder();

    // settings
    void _savePreferences(yyjson_val* values);
    void _setRunAtStartup(bool enabled);
    void _setAutoRunShout(bool enabled);
    void _onConnectivityChanged();
    // as the page shows them : starting with the system also when set from outside the app
    settings::Settings _shownSettings() const;

    // updates
    void _onUpdateChecked(const UpdateChecker::CheckResults& results);

    void _sendTrayState();

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

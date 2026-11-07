// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "src/settings/Settings.h"
#include "src/workers/base/Message.h"
#include "src/workers/heartbeat/HeartbeatThread.h"

namespace ui {

// panels of the page, over the tabs
enum class Panel { Preferences, Warnings, About };

}  // namespace ui

// What the page (src/ui/page) and the application tell each other, as JSON : the calls of the page (native.ts), the state
// and its changes (state.ts). tests/page_contract_test.cpp checks these messages against the page's types
namespace ui::page {

enum class Tab { Shout, Feeder };

//
// Calls of the page
//

// answered right away
enum class Query { Ready, Warnings };

// acted upon
enum class Command {
    RunShouts,
    StopShouts,
    RunFeeder,
    SetAutoRunShout,
    SetRunAtStartup,
    SavePreferences,
    OpenPlatform,
    OpenDataFolder,
    CheckUpdates,
    OpenRepository,
    Quit,
};

inline constexpr std::array<std::pair<Query, std::string_view>, 2> queryNames { {
    { Query::Ready, "ready" },
    { Query::Warnings, "warnings" },
} };

inline constexpr std::array<std::pair<Command, std::string_view>, 11> commandNames { {
    { Command::RunShouts, "runShouts" },
    { Command::StopShouts, "stopShouts" },
    { Command::RunFeeder, "runFeeder" },
    { Command::SetAutoRunShout, "setAutoRunShout" },
    { Command::SetRunAtStartup, "setRunAtStartup" },
    { Command::SavePreferences, "savePreferences" },
    { Command::OpenPlatform, "openPlatform" },
    { Command::OpenDataFolder, "openDataFolder" },
    { Command::CheckUpdates, "checkUpdates" },
    { Command::OpenRepository, "openRepository" },
    { Command::Quit, "quit" },
} };

// nothing if the page calls an unknown name
std::optional<Query> query(std::string_view name);
std::optional<Command> command(std::string_view name);

//
// State, answering the "ready" query
//

struct AppInfo {
    std::string name;
    std::string version;
    std::string platformName;
    std::string musicApp;
    std::string language;
    bool debug = false;
    std::string system;
    std::string copyright;
    std::string repositoryUrl;
    std::string dataFolder;
};

// how the latest check for updates ended
struct UpdateResult {
    enum class Kind { Ready, UpToDate, NotInstalled, Failed };

    Kind kind = Kind::UpToDate;
    std::string version;  // when ready
    std::string error;    // when failed
};

struct LogEntry {
    workers::Message message;
    std::int64_t time = 0;  // milliseconds since the epoch
};

// what a tab shows of its worker
struct TabState {
    std::deque<LogEntry> log;
    bool running = false;
};

struct State {
    AppInfo app;
    std::vector<std::pair<std::string, std::string>> translations;  // source text, translation
    heartbeat::Status status;
    TabState shout;
    TabState feeder;
    std::string nowPlaying;  // the last shout, as JSON : empty if none
    settings::Settings settings;
    bool canOpenPlatform = false;
    bool hasWarnings = false;
    bool searchingUpdates = false;
    std::optional<UpdateResult> updateResult;
    std::optional<Panel> panel;
};

std::string state(const State& state);

//
// State changes, as events
//

std::string statusEvent(const heartbeat::Status& status);
// appended to the tab's log, or replacing its last entry, as the message says
std::string logEvent(Tab tab, const LogEntry& entry);
// a run starts with a new log
std::string runEvent(Tab tab, bool running);
// "shout" is JSON, empty if none
std::string nowPlayingEvent(std::string_view shout);
std::string settingsEvent(const settings::Settings& settings, bool canOpenPlatform);
std::string warningsEvent(bool hasWarnings);
std::string updateEvent(bool searching, const std::optional<UpdateResult>& result);
std::string panelEvent(Panel panel);

}  // namespace ui::page

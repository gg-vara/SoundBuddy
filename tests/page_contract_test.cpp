// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The messages between the application and its page (src/app/PageMessages.h), against the page's types : every message
// the application can send, and the calls it handles, are written as TypeScript into the file given as argument, each
// value "satisfies" its type. The page-contract-types test then checks them with tsc (see tests/CMakeLists.txt)

#include <fstream>
#include <cstddef>
#include <ios>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Check.h"
#include "TestData.h"
#include "src/helpers/Platform.h"
#include "src/settings/Settings.h"
#include "src/app/PageMessages.h"
#include "src/workers/base/Message.h"
#include "src/workers/heartbeat/HeartbeatThread.h"
#include "src/workers/shout/ShoutPayload.h"

namespace {

using ui::page::Tab;

// "values", as a TypeScript array
std::string array(const std::vector<std::string>& values) {
    std::string result = "[\n";
    for (const auto& value : values) result += "  " + value + ",\n";
    return result + "]";
}

template <typename Names>
std::string names(const Names& names) {
    std::vector<std::string> quoted;
    quoted.reserve(names.size());
    for (const auto& [value, name] : names) quoted.push_back('"' + std::string { name } + '"');
    return array(quoted);
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::span arguments { argv, static_cast<std::size_t>(argc) };
    CHECK(arguments.size() == 2, "output file given");
    if (arguments.size() != 2) return check::summary();

    // calls of the page : each name maps back to its value
    for (const auto& [query, name] : ui::page::queryNames) CHECK(ui::page::query(name) == query, "query name");
    for (const auto& [command, name] : ui::page::commandNames) CHECK(ui::page::command(name) == command, "command name");
    CHECK(!ui::page::query("unknown") && !ui::page::command("unknown"), "unknown call");

    // shouts, as the watchers make them : a track, nothing playing
    const auto directory = testdata::scratchDirectory("page_contract");
    const auto audioFile = directory / "track.mp3";
    {
        std::ofstream { audioFile, std::ios::binary } << "audio";
    }
    const auto track =
        ShoutPayload {
            .playing = true,
            .positionMs = 12000,
            .duration = 215,
            .year = 1997,
            .fileLocation = platform::toUtf8(audioFile),
            .name = "Track",
            .album = "Album",
            .artist = "Artist",
            .genre = "Genre",
            .skippedDate = {},
            .playedDate = {},
        }
            .toJSON();
    const auto nothing = ShoutPayload {}.toJSON();
    CHECK(!track.isEmpty && nothing.isEmpty, "track and empty shouts");

    const settings::Settings settings {
        .platformHostUrl = "https://example.com",
        .platformUsername = "user",
        .platformPassword = "password",
        .autoRunShout = true,
        .runAtStartup = false,
    };

    // events : every variant
    std::vector<std::string> events;
    for (const auto connectivity :
         { heartbeat::Connectivity::Refused, heartbeat::Connectivity::Checking, heartbeat::Connectivity::Connected }) {
        events.push_back(ui::page::statusEvent({ .text = "Status", .connectivity = connectivity }));
    }
    for (const auto tab : { Tab::Shout, Tab::Feeder }) {
        for (const auto kind : { workers::MessageKind::Standard, workers::MessageKind::Warning, workers::MessageKind::Issue }) {
            for (const auto line : { workers::LineMode::Append, workers::LineMode::ReplaceLast }) {
                const ui::page::LogEntry entry { .message = { .text = "Message", .kind = kind, .line = line }, .time = 1700000000000 };
                events.push_back(ui::page::logEvent(tab, entry));
            }
        }
        events.push_back(ui::page::runEvent(tab, /*running=*/true));
        events.push_back(ui::page::runEvent(tab, /*running=*/false));
    }
    events.push_back(ui::page::nowPlayingEvent(track.json));
    events.push_back(ui::page::nowPlayingEvent(nothing.json));
    events.push_back(ui::page::nowPlayingEvent({}));
    events.push_back(ui::page::settingsEvent(settings, /*canOpenPlatform=*/true));
    events.push_back(ui::page::warningsEvent(/*hasWarnings=*/true));
    events.push_back(ui::page::updateEvent(/*searching=*/true, std::nullopt));
    using Kind = ui::page::UpdateResult::Kind;
    for (const auto kind : { Kind::Ready, Kind::UpToDate, Kind::NotInstalled, Kind::Failed }) {
        events.push_back(
            ui::page::updateEvent(/*searching=*/false, ui::page::UpdateResult { .kind = kind, .version = "1.0.0", .error = "Error" })
        );
    }
    events.push_back(ui::page::panelEvent(ui::Panel::Preferences));
    events.push_back(ui::page::panelEvent(ui::Panel::Warnings));
    events.push_back(ui::page::panelEvent(ui::Panel::About));

    // states : a page loading while the app runs, then just started
    const ui::page::State running {
        .app = { .name = "SoundBuddy",
                 .version = "1.0.0",
                 .platformName = "SoundVitrine",
                 .musicApp = "iTunes",
                 .language = "en",
                 .debug = true,
                 .system = "Windows (x64)",
                 .copyright = "Copyright",
                 .repositoryUrl = "https://example.invalid",
                 .dataFolder = "folder" },
        .translations = { { "Shout", "Crier" } },
        .status = { .text = "Connected", .connectivity = heartbeat::Connectivity::Connected },
        .shout = { .log = { { .message = { .text = "Shouting", .kind = workers::MessageKind::Standard }, .time = 1700000000000 } },
                   .running = true },
        .feeder = {},
        .nowPlaying = track.json,
        .settings = settings,
        .canOpenPlatform = true,
        .hasWarnings = true,
        .searchingUpdates = true,
        .updateResult = ui::page::UpdateResult { .kind = Kind::Failed, .version = {}, .error = "Error" },
        .panel = ui::Panel::Warnings,
    };
    const ui::page::State started {};

    const auto contract =
        std::string { "// Generated by tests/page_contract_test.cpp : checked by tsc against the page's types\n" } +
        "import type { CommandName, QueryName } from '" PAGE_SOURCE_DIR "/native';\n" +
        "import type { NativeEvent, NativeState } from '" PAGE_SOURCE_DIR "/state';\n\n" +
        "export const states = " + array({ ui::page::state(running), ui::page::state(started) }) + " satisfies NativeState[];\n\n" +
        "export const events = " + array(events) + " satisfies NativeEvent[];\n\n" +
        "// the calls the application handles, which are all of the page's : a call left out is reported missing from these\n" +
        "export const queries = " + names(ui::page::queryNames) + " as const satisfies readonly QueryName[];\n" +
        "export const commands = " + names(ui::page::commandNames) + " as const satisfies readonly CommandName[];\n" +
        "export const unhandledQueries: Record<Exclude<QueryName, (typeof queries)[number]>, never> = {};\n" +
        "export const unhandledCommands: Record<Exclude<CommandName, (typeof commands)[number]>, never> = {};\n";

    std::ofstream output { platform::fromUtf8(arguments[1]), std::ios::binary };
    output << contract;
    output.close();
    CHECK(!output.fail(), "contract written");

    testdata::removeScratchDirectories();
    return check::summary();
}

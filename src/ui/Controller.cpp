// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Controller.h"

#include <yyjson.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "src/helpers/Json.h"
#include "src/helpers/Platform.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/settings/Settings.h"
#include "src/ui/PageMessages.h"
#include "src/ui/Post.h"
#include "src/workers/base/Message.h"
#include "src/workers/feeder/FeederThread.h"
#include "src/workers/mBeat/MBeatThread.h"
#include "version.h"

namespace ui {

namespace {

// as many messages as a tab keeps
constexpr std::size_t maxLogEntries = 100;

#ifdef _DEBUG
constexpr bool isDebugBuild = true;
#else
constexpr bool isDebugBuild = false;
#endif

#ifdef _WIN32
constexpr std::string_view systemName = "Windows";
#else
constexpr std::string_view systemName = "macOS";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
constexpr std::string_view architecture = "arm64";
#else
constexpr std::string_view architecture = "x64";
#endif

std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// [name, argument] of a page call
struct PageCall {
    json::Document doc;
    std::string_view name;
    yyjson_val* argument;
};

PageCall parseCall(std::string_view request) {
    auto doc = json::parse(request);
    auto* const arguments = json::root(doc);
    const auto name = json::string(yyjson_arr_get(arguments, 0));
    auto* const argument = yyjson_arr_get(arguments, 1);
    return { .doc = std::move(doc), .name = name, .argument = argument };
}

}  // namespace

Controller::Controller(Shell& shell) :
    _shell(shell) {
    _runMBeat();
    checkForUpdates();

    if (_appSettings.values().autoRunShout) _runShouts();
}

Controller::~Controller() {
    // the watcher stops first, as it feeds the uploader
    _shoutWorker.reset();
    _shoutUploader.reset();
    _feederWorker.reset();
    _mbeatWorker.reset();
}

//
// Page calls
//

std::string Controller::query(std::string_view request) {
    const auto call = parseCall(request);

    const auto query = page::query(call.name);
    if (!query) {
        logs::warning("UI", "Unknown page query \"{}\"", call.name);
        return "null";
    }

    switch (*query) {
        case page::Query::Ready:
            return _state(call.argument);

        // the warnings of the latest library export : an array of tracks, each an object of texts
        case page::Query::Warnings: {
            std::ifstream warnings { AppSettings::getFeedWarningFilePath(), std::ios::binary };
            const std::string content { std::istreambuf_iterator<char>(warnings), std::istreambuf_iterator<char>() };
            return yyjson_is_arr(json::root(json::parse(content))) ? content : "[]";
        }
    }

    return "null";
}

void Controller::command(std::string_view request) {
    const auto call = parseCall(request);

    const auto command = page::command(call.name);
    if (!command) {
        logs::warning("UI", "Unknown page command \"{}\"", call.name);
        return;
    }

    switch (*command) {
        case page::Command::RunShouts:
            _runShouts();
            break;
        case page::Command::StopShouts:
            _stopShouts();
            break;
        case page::Command::RunFeeder:
            _runFeeder();
            break;
        case page::Command::SetAutoRunShout:
            _setAutoRunShout(json::boolean(call.argument));
            break;
        case page::Command::SetRunAtStartup:
            _setRunAtStartup(json::boolean(call.argument));
            break;
        case page::Command::SavePreferences:
            _savePreferences(call.argument);
            break;
        case page::Command::OpenPlatform:
            openPlatform();
            break;
        case page::Command::OpenDataFolder:
            openDataFolder();
            break;
        case page::Command::CheckUpdates:
            checkForUpdates();
            break;
        case page::Command::OpenRepository:
            platform::openUrl(APP_UPDATES_REPOSITORY_URL);
            break;
        case page::Command::Quit:
            requestQuit();
            break;
    }
}

std::string Controller::_state(yyjson_val* translationKeys) {
    page::State state {
        .app = {
            .name = APP_NAME,
            .version = APP_CURRENT_VERSION,
            .platformName = DEST_PLATFORM_PRODUCT_NAME,
            .musicApp = std::string { platform::musicAppName() },
            .language = std::string { i18n::languageCode() },
            .debug = isDebugBuild,
            .system = std::format("{} ({})", systemName, architecture),
            .copyright = APP_COPYRIGHT,
            .repositoryUrl = APP_UPDATES_REPOSITORY_URL,
            .dataFolder = platform::toUtf8(platform::dataDirectory()),
        },
        .translations = {},
        .status = _status,
        .shout = _shoutTab.shown,
        .feeder = _feederTab.shown,
        .nowPlaying = _nowPlaying,
        .settings = _shownSettings(),
        .canOpenPlatform = canOpenPlatform(),
        .hasWarnings = hasWarnings(),
        .searchingUpdates = _searchingUpdates,
        .updateResult = _updateResult,
        .panel = _pendingPanel,
    };
    _pendingPanel.reset();

    // texts of the page, in the user interface language
    std::size_t index = 0;
    std::size_t count = 0;
    yyjson_val* key = nullptr;
    yyjson_arr_foreach(translationKeys, index, count, key) {
        const auto source = json::string(key);
        state.translations.emplace_back(source, i18n::tr(source));
    }

    return page::state(state);
}

settings::Settings Controller::_shownSettings() const {
    auto shown = _appSettings.values();
    shown.runAtStartup = shown.runAtStartup || platform::isLaunchingAtStartup();
    return shown;
}

//
// Native menus
//

void Controller::openPanel(Panel panel) {
    // a page loading meanwhile finds it in the state
    if (!_shell.sendToPage(page::panelEvent(panel))) _pendingPanel = panel;

    _shell.showWindow();
}

void Controller::openPlatform() {
    const auto platformHomeUrl = _appSettings.getConnectivityInfos().getPlaformHomeUrl();
    if (platformHomeUrl.empty()) {
        _shell.tell(
            i18n::format(i18n::tr("Cannot access %1 plaform"), { DEST_PLATFORM_PRODUCT_NAME }),
            i18n::format(
                i18n::tr("%1 Plaform URL is not valid. Please check Preferences connectivity parameters."), { DEST_PLATFORM_PRODUCT_NAME }
            ),
            Severity::Error
        );
        return;
    }

    platform::openUrl(platformHomeUrl);
}

void Controller::openDataFolder() {
    platform::openPath(platform::dataDirectory());
}

std::string Controller::versionLabel() {
    return std::string { APP_NAME " " APP_CURRENT_VERSION } + (isDebugBuild ? " (Debug)" : "");
}

void Controller::checkForUpdates() {
    _searchingUpdates = true;
    _sendUpdate();

    _updateChecker.check([this](const UpdateChecker::CheckResults& results) { post([this, results] { _onUpdateChecked(results); }); });
}

void Controller::requestQuit() {
    // already asking
    if (_quitting) return;
    _quitting = true;

    if (_shoutWorker && _shoutWorker->isRunning()) {
        if (!_shell.ask(
                i18n::tr("Shout worker running !"), i18n::tr("Shout worker is actually running : Are you sure you want to exit ?"),
                Answer::No
            )) {
            _quitting = false;
            return;
        }

        // makes sure to wait for the watcher to end : limits COM app retention on Windows
        _shoutWorker.reset();

        // the last shout ("nothing playing") gets a chance to be uploaded
        if (_shoutUploader) _shoutUploader->waitForDrain(std::chrono::seconds { 1 });
    }

    // stops the heartbeat, aborting its request in progress if any
    _mbeatWorker.reset();

    _shell.quit();
}

bool Controller::canOpenPlatform() const {
    return !_appSettings.getConnectivityInfos().getPlaformHomeUrl().empty();
}

bool Controller::hasWarnings() {
    std::error_code error;
    return std::filesystem::exists(AppSettings::getFeedWarningFilePath(), error);
}

//
// Workers
//

workers::MessageSink Controller::_messageSink(Tab tab) {
    return [this, tab](const workers::Message& message) { post([this, tab, message] { _log(tab, message); }); };
}

void Controller::_runMBeat() {
    // the previous heartbeat stops first
    _mbeatWorker.reset();

    _mbeatWorker = std::make_unique<MBeatThread>(_appSettings.getConnectivityInfos(), [this](const mbeat::Status& status) {
        post([this, status] {
            _status = status;

            _shell.sendToPage(page::statusEvent(status));

            // while checking, the last answer stands : the heartbeat would otherwise make the dot blink
            if (status.connectivity != mbeat::Connectivity::Checking) {
                _trayDot = status.connectivity == mbeat::Connectivity::Connected ? TrayState::Dot::Ok : TrayState::Dot::Issue;
                _sendTrayState();
            }
        });
    });
}

void Controller::_runShouts() {
    _stopShouts();

    const auto run = _runStarted(Tab::Shout);

    _shoutUploader = std::make_unique<ShoutUploader>(_appSettings.getConnectivityInfos(), _messageSink(Tab::Shout));

    // the watcher always stops before the uploader is destroyed : it hands shouts over directly
    _shoutWorker = std::make_unique<ShoutWatcher>(ShoutWatcher::Callbacks {
        .onMessage = _messageSink(Tab::Shout),
        .onShout =
            [this, uploader = _shoutUploader.get()](std::string shoutAsJson) {
                uploader->upload(shoutAsJson);
                post([this, shout = std::move(shoutAsJson)] {
                    _nowPlaying = shout;
                    _shell.sendToPage(page::nowPlayingEvent(shout));
                });
            },
        .onFinished = [this, run] { post([this, run] { _runFinished(Tab::Shout, run); }); },
    });
}

void Controller::_stopShouts() {
    // the watcher stops first, as it feeds the uploader
    _shoutWorker.reset();

    if (_shoutUploader) {
        _shoutUploader->waitForDrain(std::chrono::seconds { 1 });
        _shoutUploader.reset();
    }
}

void Controller::_runFeeder() {
    // the previous run stops first
    _feederWorker.reset();

    const auto run = _runStarted(Tab::Feeder);

    _feederWorker = std::make_unique<FeederThread>(
        _appSettings.getConnectivityInfos(), FeederThread::Callbacks {
                                                 .onMessage = _messageSink(Tab::Feeder),
                                                 .onFilesGenerated = [this] { post([this] { _sendWarnings(); }); },
                                                 .onFinished = [this, run] { post([this, run] { _runFinished(Tab::Feeder, run); }); },
                                             }
    );
}

//
// Settings
//

void Controller::_savePreferences(yyjson_val* values) {
    auto& settings = _appSettings.values();
    settings.platformHostUrl = json::string(yyjson_obj_get(values, "platformHostUrl"));
    settings.platformUsername = json::string(yyjson_obj_get(values, "platformUsername"));
    settings.platformPassword = json::string(yyjson_obj_get(values, "platformPassword"));
    _appSettings.save();

    _sendSettings();
    _onConnectivityChanged();
}

void Controller::_setRunAtStartup(bool enabled) {
    platform::setLaunchingAtStartup(enabled);
    _appSettings.values().runAtStartup = enabled;
    _appSettings.save();
    _sendSettings();
}

void Controller::_setAutoRunShout(bool enabled) {
    _appSettings.values().autoRunShout = enabled;
    _appSettings.save();
    _sendSettings();
}

void Controller::_onConnectivityChanged() {
    // workers restart with the new connectivity infos
    _runMBeat();
    if (_shoutWorker && _shoutWorker->isRunning()) _runShouts();
    if (_feederWorker && _feederWorker->isRunning()) _runFeeder();
}

//
// Updates
//

void Controller::_onUpdateChecked(const UpdateChecker::CheckResults& results) {
    using Kind = page::UpdateResult::Kind;
    const auto kindOf = [](UpdateChecker::Result result) {
        switch (result) {
            case UpdateChecker::Result::NoUpdate:
                break;
            case UpdateChecker::Result::UpdateReady:
                return Kind::Ready;
            case UpdateChecker::Result::NotInstalled:
                return Kind::NotInstalled;
            case UpdateChecker::Result::Failed:
                return Kind::Failed;
        }
        return Kind::UpToDate;
    };

    // told by the About panel, as the check may not have been asked for
    _searchingUpdates = false;
    _updateResult = page::UpdateResult { .kind = kindOf(results.result), .version = results.version, .error = results.error };
    _sendUpdate();

    if (results.result != UpdateChecker::Result::UpdateReady) return;

    // update downloaded : if declined, it is applied on next launch anyway
    const auto accepted = _shell.ask(
        i18n::tr("Update Available"),
        i18n::format(
            i18n::tr("%1 %2 is ready to be installed. Would you like to restart %1 now to install it ?"), { APP_NAME, results.version }
        ),
        Answer::Yes
    );

    if (accepted) {
        _updateChecker.applyUpdateOnExit();
        requestQuit();
    }
}

//
// State changes
//

void Controller::_log(Tab tab, const workers::Message& message) {
    auto& log = _tab(tab).shown.log;
    const page::LogEntry entry { .message = message, .time = now() };

    if (message.line == workers::LineMode::ReplaceLast && !log.empty()) {
        log.back() = entry;
    } else {
        log.push_back(entry);
        if (log.size() > maxLogEntries) log.pop_front();
    }

    _shell.sendToPage(page::logEvent(tab, entry));
}

int Controller::_runStarted(Tab tab) {
    auto& state = _tab(tab);
    state.shown.log.clear();
    state.shown.running = true;
    _shell.sendToPage(page::runEvent(tab, /*running=*/true));
    if (tab == Tab::Shout) _sendTrayState();

    return ++state.run;
}

void Controller::_runFinished(Tab tab, int run) {
    // a stopped run finishes after its replacement started
    auto& state = _tab(tab);
    if (run != state.run) return;
    state.shown.running = false;
    _shell.sendToPage(page::runEvent(tab, /*running=*/false));
    if (tab == Tab::Shout) _sendTrayState();
}

void Controller::_sendTrayState() {
    _shell.setTrayState({ .shouting = _shoutTab.shown.running, .dot = _trayDot });
}

void Controller::_sendSettings() {
    _shell.sendToPage(page::settingsEvent(_shownSettings(), canOpenPlatform()));
}

void Controller::_sendWarnings() {
    _shell.sendToPage(page::warningsEvent(hasWarnings()));
}

void Controller::_sendUpdate() {
    _shell.sendToPage(page::updateEvent(_searchingUpdates, _updateResult));
}

}  // namespace ui

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Controller.h"

#include <yyjson.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
#include "src/ui/Post.h"
#include "src/workers/base/Message.h"
#include "src/workers/feeder/FeederThread.h"
#include "src/workers/mBeat/MBeatThread.h"
#include "version.h"

namespace ui {

namespace {

// as many messages as a tab keeps
constexpr std::size_t maxLogEntries = 100;

const char* kindName(workers::MessageKind kind) {
    switch (kind) {
        case workers::MessageKind::Standard:
            break;
        case workers::MessageKind::Issue:
            return "issue";
        case workers::MessageKind::Warning:
            return "warning";
    }
    return "standard";
}

const char* connectivityName(mbeat::Connectivity connectivity) {
    switch (connectivity) {
        case mbeat::Connectivity::Refused:
            break;
        case mbeat::Connectivity::Checking:
            return "checking";
        case mbeat::Connectivity::Connected:
            return "connected";
    }
    return "refused";
}

const char* panelName(Panel panel) {
    return panel == Panel::Preferences ? "preferences" : "warnings";
}

std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// JSON object under construction : keys must be literals, values are copied
class JsonObject {
 public:
    JsonObject() :
        _doc(json::create()),
        _root(yyjson_mut_obj(_doc.get())) {
        yyjson_mut_doc_set_root(_doc.get(), _root);
    }

    yyjson_mut_doc* doc() const { return _doc.get(); }
    yyjson_mut_val* root() const { return _root; }

    std::string write() const { return json::write(_doc); }

 private:
    json::MutableDocument _doc;
    yyjson_mut_val* _root;
};

void addString(yyjson_mut_doc* doc, yyjson_mut_val* object, const char* key, std::string_view value) {
    yyjson_mut_obj_add_strncpy(doc, object, key, value.data(), value.size());
}

// "value" is already JSON, or empty for null
void addRaw(yyjson_mut_doc* doc, yyjson_mut_val* object, const char* key, std::string_view value) {
    if (value.empty()) {
        yyjson_mut_obj_add_null(doc, object, key);
    } else {
        yyjson_mut_obj_add_val(doc, object, key, yyjson_mut_rawncpy(doc, value.data(), value.size()));
    }
}

// new event for the page
JsonObject event(const char* type) {
    JsonObject object;
    addString(object.doc(), object.root(), "type", type);
    return object;
}

yyjson_mut_val* messageValue(yyjson_mut_doc* doc, const workers::Message& message, std::int64_t time) {
    auto* const value = yyjson_mut_obj(doc);
    addString(doc, value, "text", message.text);
    addString(doc, value, "kind", kindName(message.kind));
    yyjson_mut_obj_add_int(doc, value, "time", time);
    return value;
}

yyjson_mut_val* statusValue(yyjson_mut_doc* doc, const mbeat::Status& status) {
    auto* const value = yyjson_mut_obj(doc);
    addString(doc, value, "text", status.text);
    addString(doc, value, "connectivity", connectivityName(status.connectivity));
    return value;
}

yyjson_mut_val* settingsValue(yyjson_mut_doc* doc, const settings::Settings& settings) {
    auto* const value = yyjson_mut_obj(doc);
    addString(doc, value, "platformHostUrl", settings.platformHostUrl);
    addString(doc, value, "platformUsername", settings.platformUsername);
    addString(doc, value, "platformPassword", settings.platformPassword);
    yyjson_mut_obj_add_bool(doc, value, "autoRunShout", settings.autoRunShout);
    yyjson_mut_obj_add_bool(doc, value, "runAtStartup", settings.runAtStartup || platform::isLaunchingAtStartup());
    return value;
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
    _checkForUpdates(/*userAsked=*/false);

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

    if (call.name == "ready") return _state(call.argument);

    // the warnings of the latest library export : an array of tracks, each an object of texts
    if (call.name == "warnings") {
        std::ifstream warnings { AppSettings::getFeedWarningFilePath(), std::ios::binary };
        const std::string content { std::istreambuf_iterator<char>(warnings), std::istreambuf_iterator<char>() };
        return yyjson_is_arr(json::root(json::parse(content))) ? content : "[]";
    }

    logs::warning("UI", "Unknown page query \"{}\"", call.name);
    return "null";
}

void Controller::command(std::string_view request) {
    const auto call = parseCall(request);
    const auto& name = call.name;

    if (name == "runShouts") {
        _runShouts();
    } else if (name == "stopShouts") {
        _stopShouts();
    } else if (name == "runFeeder") {
        _runFeeder();
    } else if (name == "setAutoRunShout") {
        _setAutoRunShout(json::boolean(call.argument));
    } else if (name == "setRunAtStartup") {
        _setRunAtStartup(json::boolean(call.argument));
    } else if (name == "savePreferences") {
        _savePreferences(call.argument);
    } else if (name == "openPlatform") {
        openPlatform();
    } else if (name == "openDataFolder") {
        openDataFolder();
    } else if (name == "checkUpdates") {
        checkForUpdates();
    } else if (name == "quit") {
        requestQuit();
    } else {
        logs::warning("UI", "Unknown page command \"{}\"", name);
    }
}

std::string Controller::_state(yyjson_val* translationKeys) {
    const JsonObject state;
    auto* const doc = state.doc();
    auto* const root = state.root();

    auto* const app = yyjson_mut_obj_add_obj(doc, root, "app");
    addString(doc, app, "name", APP_NAME);
    addString(doc, app, "version", APP_CURRENT_VERSION);
    addString(doc, app, "platformName", DEST_PLATFORM_PRODUCT_NAME);
    addString(doc, app, "musicApp", platform::musicAppName());
    addString(doc, app, "language", i18n::languageCode());
#ifdef _DEBUG
    yyjson_mut_obj_add_bool(doc, app, "debug", /*val=*/true);
#else
    yyjson_mut_obj_add_bool(doc, app, "debug", /*val=*/false);
#endif

    // texts of the page, in the user interface language
    auto* const translations = yyjson_mut_obj_add_obj(doc, root, "translations");
    std::size_t index = 0;
    std::size_t count = 0;
    yyjson_val* key = nullptr;
    yyjson_arr_foreach(translationKeys, index, count, key) {
        const auto source = json::string(key);
        const auto translation = i18n::tr(source);
        yyjson_mut_obj_add(
            translations, yyjson_mut_strncpy(doc, source.data(), source.size()),
            yyjson_mut_strncpy(doc, translation.data(), translation.size())
        );
    }

    yyjson_mut_obj_add_val(doc, root, "status", statusValue(doc, _status));

    auto* const tabs = yyjson_mut_obj_add_obj(doc, root, "tabs");
    for (const auto tab : { Tab::Shout, Tab::Feeder }) {
        const auto& tabState = _tab(tab);
        auto* const value = yyjson_mut_obj_add_obj(doc, tabs, _tabName(tab));
        yyjson_mut_obj_add_bool(doc, value, "running", tabState.running);
        auto* const log = yyjson_mut_obj_add_arr(doc, value, "log");
        for (const auto& entry : tabState.log) yyjson_mut_arr_append(log, messageValue(doc, entry.message, entry.time));
    }

    addRaw(doc, root, "nowPlaying", _nowPlaying);
    yyjson_mut_obj_add_val(doc, root, "settings", settingsValue(doc, _appSettings.values()));
    yyjson_mut_obj_add_bool(doc, root, "canOpenPlatform", canOpenPlatform());
    yyjson_mut_obj_add_bool(doc, root, "hasWarnings", hasWarnings());
    yyjson_mut_obj_add_bool(doc, root, "searchingUpdates", _searchingUpdates);

    if (_pendingPanel) {
        addString(doc, root, "panel", panelName(*_pendingPanel));
        _pendingPanel.reset();
    } else {
        yyjson_mut_obj_add_null(doc, root, "panel");
    }

    return state.write();
}

//
// Native menus
//

void Controller::openPanel(Panel panel) {
    const auto opened = event("panel");
    addString(opened.doc(), opened.root(), "panel", panelName(panel));

    // a page loading meanwhile finds it in the state
    if (!_shell.sendToPage(opened.write())) _pendingPanel = panel;

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

void Controller::checkForUpdates() {
    _checkForUpdates(/*userAsked=*/true);
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

            const auto changed = event("status");
            yyjson_mut_obj_add_val(changed.doc(), changed.root(), "status", statusValue(changed.doc(), status));
            _shell.sendToPage(changed.write());
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

                    const auto played = event("nowPlaying");
                    addRaw(played.doc(), played.root(), "shout", shout);
                    _shell.sendToPage(played.write());
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

void Controller::_checkForUpdates(bool userAsked) {
    // a check already in progress notifies the user as well
    _userAskedUpdates = _userAskedUpdates || userAsked;
    _searchingUpdates = true;
    _sendUpdate();

    _updateChecker.check([this](const UpdateChecker::CheckResults& results) { post([this, results] { _onUpdateChecked(results); }); });
}

void Controller::_onUpdateChecked(const UpdateChecker::CheckResults& results) {
    const bool userAsked = _userAskedUpdates;
    _userAskedUpdates = false;
    _searchingUpdates = false;
    _sendUpdate();

    switch (results.result) {
        case UpdateChecker::Result::UpdateReady:
            break;

        case UpdateChecker::Result::NoUpdate:
            if (userAsked) _shell.tell(i18n::tr("Checking updates"), i18n::tr("No updates available at the time."), Severity::Information);
            return;

        case UpdateChecker::Result::NotInstalled:
            if (userAsked) {
                _shell.tell(
                    i18n::tr("Checking updates"),
                    i18n::format(i18n::tr("Updates are only available for installed versions of %1."), { APP_NAME }), Severity::Information
                );
            }
            return;

        case UpdateChecker::Result::Failed:
            if (userAsked) _shell.tell(i18n::tr("Error while checking updates"), results.error, Severity::Error);
            return;
    }

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
    auto& log = _tab(tab).log;
    const auto time = now();

    if (message.line == workers::LineMode::ReplaceLast && !log.empty()) {
        log.back() = { .message = message, .time = time };
    } else {
        log.push_back({ .message = message, .time = time });
        if (log.size() > maxLogEntries) log.pop_front();
    }

    const auto logged = event("log");
    addString(logged.doc(), logged.root(), "tab", _tabName(tab));
    yyjson_mut_obj_add_val(logged.doc(), logged.root(), "message", messageValue(logged.doc(), message, time));
    yyjson_mut_obj_add_bool(logged.doc(), logged.root(), "replaceLast", message.line == workers::LineMode::ReplaceLast);
    _shell.sendToPage(logged.write());
}

int Controller::_runStarted(Tab tab) {
    auto& state = _tab(tab);
    state.log.clear();
    state.running = true;

    const auto started = event("run");
    addString(started.doc(), started.root(), "tab", _tabName(tab));
    yyjson_mut_obj_add_bool(started.doc(), started.root(), "running", /*val=*/true);
    yyjson_mut_obj_add_bool(started.doc(), started.root(), "newLog", /*val=*/true);
    _shell.sendToPage(started.write());

    return ++state.run;
}

void Controller::_runFinished(Tab tab, int run) {
    // a stopped run finishes after its replacement started
    auto& state = _tab(tab);
    if (run != state.run) return;
    state.running = false;

    const auto finished = event("run");
    addString(finished.doc(), finished.root(), "tab", _tabName(tab));
    yyjson_mut_obj_add_bool(finished.doc(), finished.root(), "running", /*val=*/false);
    yyjson_mut_obj_add_bool(finished.doc(), finished.root(), "newLog", /*val=*/false);
    _shell.sendToPage(finished.write());
}

void Controller::_sendSettings() {
    const auto changed = event("settings");
    yyjson_mut_obj_add_val(changed.doc(), changed.root(), "settings", settingsValue(changed.doc(), _appSettings.values()));
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "canOpenPlatform", canOpenPlatform());
    _shell.sendToPage(changed.write());
}

void Controller::_sendWarnings() {
    const auto changed = event("warnings");
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "hasWarnings", hasWarnings());
    _shell.sendToPage(changed.write());
}

void Controller::_sendUpdate() {
    const auto changed = event("update");
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "searching", _searchingUpdates);
    _shell.sendToPage(changed.write());
}

}  // namespace ui

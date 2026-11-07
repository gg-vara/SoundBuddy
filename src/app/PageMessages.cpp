// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PageMessages.h"

#include <yyjson.h>

#include <optional>
#include <string>
#include <string_view>

#include "src/helpers/Json.h"
#include "src/settings/Settings.h"
#include "src/workers/base/Message.h"
#include "src/workers/heartbeat/HeartbeatThread.h"

namespace ui::page {

namespace {

const char* tabName(Tab tab) {
    return tab == Tab::Shout ? "shout" : "feeder";
}

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

const char* connectivityName(heartbeat::Connectivity connectivity) {
    switch (connectivity) {
        case heartbeat::Connectivity::Refused:
            break;
        case heartbeat::Connectivity::Unreachable:
            return "unreachable";
        case heartbeat::Connectivity::Checking:
            return "checking";
        case heartbeat::Connectivity::Connected:
            return "connected";
    }
    return "refused";
}

const char* updateKindName(UpdateResult::Kind kind) {
    switch (kind) {
        case UpdateResult::Kind::UpToDate:
            break;
        case UpdateResult::Kind::Ready:
            return "ready";
        case UpdateResult::Kind::NotInstalled:
            return "notInstalled";
        case UpdateResult::Kind::Failed:
            return "failed";
    }
    return "upToDate";
}

const char* panelName(Panel panel) {
    switch (panel) {
        case Panel::Preferences:
            break;
        case Panel::Warnings:
            return "warnings";
        case Panel::About:
            return "about";
    }
    return "preferences";
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

yyjson_mut_val* logEntryValue(yyjson_mut_doc* doc, const LogEntry& entry) {
    auto* const value = yyjson_mut_obj(doc);
    addString(doc, value, "text", entry.message.text);
    addString(doc, value, "kind", kindName(entry.message.kind));
    yyjson_mut_obj_add_int(doc, value, "time", entry.time);
    return value;
}

yyjson_mut_val* statusValue(yyjson_mut_doc* doc, const heartbeat::Status& status) {
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
    yyjson_mut_obj_add_bool(doc, value, "runAtStartup", settings.runAtStartup);
    return value;
}

// null if there was no check yet
yyjson_mut_val* updateResultValue(yyjson_mut_doc* doc, const std::optional<UpdateResult>& result) {
    if (!result) return yyjson_mut_null(doc);

    auto* const value = yyjson_mut_obj(doc);
    addString(doc, value, "kind", updateKindName(result->kind));
    addString(doc, value, "version", result->version);
    addString(doc, value, "error", result->error);
    return value;
}

yyjson_mut_val* tabValue(yyjson_mut_doc* doc, const TabState& tab) {
    auto* const value = yyjson_mut_obj(doc);
    yyjson_mut_obj_add_bool(doc, value, "running", tab.running);
    auto* const log = yyjson_mut_obj_add_arr(doc, value, "log");
    for (const auto& entry : tab.log) yyjson_mut_arr_append(log, logEntryValue(doc, entry));
    return value;
}

}  // namespace

//
// Calls of the page
//

std::optional<Query> query(std::string_view name) {
    for (const auto& [query, queryName] : queryNames) {
        if (queryName == name) return query;
    }
    return std::nullopt;
}

std::optional<Command> command(std::string_view name) {
    for (const auto& [command, commandName] : commandNames) {
        if (commandName == name) return command;
    }
    return std::nullopt;
}

//
// State
//

std::string state(const State& state) {
    const JsonObject object;
    auto* const doc = object.doc();
    auto* const root = object.root();

    auto* const app = yyjson_mut_obj_add_obj(doc, root, "app");
    addString(doc, app, "name", state.app.name);
    addString(doc, app, "version", state.app.version);
    addString(doc, app, "platformName", state.app.platformName);
    addString(doc, app, "musicApp", state.app.musicApp);
    addString(doc, app, "language", state.app.language);
    yyjson_mut_obj_add_bool(doc, app, "debug", state.app.debug);
    addString(doc, app, "system", state.app.system);
    addString(doc, app, "copyright", state.app.copyright);
    addString(doc, app, "repositoryUrl", state.app.repositoryUrl);
    addString(doc, app, "dataFolder", state.app.dataFolder);

    auto* const translations = yyjson_mut_obj_add_obj(doc, root, "translations");
    for (const auto& [source, translation] : state.translations) {
        yyjson_mut_obj_add(
            translations, yyjson_mut_strncpy(doc, source.data(), source.size()),
            yyjson_mut_strncpy(doc, translation.data(), translation.size())
        );
    }

    yyjson_mut_obj_add_val(doc, root, "status", statusValue(doc, state.status));

    auto* const tabs = yyjson_mut_obj_add_obj(doc, root, "tabs");
    yyjson_mut_obj_add_val(doc, tabs, tabName(Tab::Shout), tabValue(doc, state.shout));
    yyjson_mut_obj_add_val(doc, tabs, tabName(Tab::Feeder), tabValue(doc, state.feeder));

    addRaw(doc, root, "nowPlaying", state.nowPlaying);
    yyjson_mut_obj_add_val(doc, root, "settings", settingsValue(doc, state.settings));
    yyjson_mut_obj_add_bool(doc, root, "canOpenPlatform", state.canOpenPlatform);
    yyjson_mut_obj_add_bool(doc, root, "hasWarnings", state.hasWarnings);
    yyjson_mut_obj_add_bool(doc, root, "searchingUpdates", state.searchingUpdates);
    yyjson_mut_obj_add_val(doc, root, "updateResult", updateResultValue(doc, state.updateResult));

    return object.write();
}

//
// State changes
//

std::string statusEvent(const heartbeat::Status& status) {
    const auto changed = event("status");
    yyjson_mut_obj_add_val(changed.doc(), changed.root(), "status", statusValue(changed.doc(), status));
    return changed.write();
}

std::string logEvent(Tab tab, const LogEntry& entry) {
    const auto logged = event("log");
    addString(logged.doc(), logged.root(), "tab", tabName(tab));
    yyjson_mut_obj_add_val(logged.doc(), logged.root(), "message", logEntryValue(logged.doc(), entry));
    yyjson_mut_obj_add_bool(logged.doc(), logged.root(), "replaceLast", entry.message.line == workers::LineMode::ReplaceLast);
    return logged.write();
}

std::string runEvent(Tab tab, bool running) {
    const auto changed = event("run");
    addString(changed.doc(), changed.root(), "tab", tabName(tab));
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "running", running);
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "newLog", running);
    return changed.write();
}

std::string nowPlayingEvent(std::string_view shout) {
    const auto played = event("nowPlaying");
    addRaw(played.doc(), played.root(), "shout", shout);
    return played.write();
}

std::string settingsEvent(const settings::Settings& settings, bool canOpenPlatform) {
    const auto changed = event("settings");
    yyjson_mut_obj_add_val(changed.doc(), changed.root(), "settings", settingsValue(changed.doc(), settings));
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "canOpenPlatform", canOpenPlatform);
    return changed.write();
}

std::string warningsEvent(bool hasWarnings) {
    const auto changed = event("warnings");
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "hasWarnings", hasWarnings);
    return changed.write();
}

std::string updateEvent(bool searching, const std::optional<UpdateResult>& result) {
    const auto changed = event("update");
    yyjson_mut_obj_add_bool(changed.doc(), changed.root(), "searching", searching);
    yyjson_mut_obj_add_val(changed.doc(), changed.root(), "result", updateResultValue(changed.doc(), result));
    return changed.write();
}

std::string panelEvent(Panel panel) {
    const auto opened = event("panel");
    addString(opened.doc(), opened.root(), "panel", panelName(panel));
    return opened.write();
}

}  // namespace ui::page

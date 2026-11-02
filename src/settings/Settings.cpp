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

#include "Settings.h"

#include <fstream>
#include <iterator>
#include <system_error>

#include "src/helpers/Json.hpp"

namespace settings {

namespace {

constexpr const char* platformHostUrl = "platformHostUrl";
constexpr const char* platformUsername = "platformUsername";
constexpr const char* platformPassword = "platformPassword";
constexpr const char* autoRunShout = "autoRunShout";
constexpr const char* runAtStartup = "runAtStartup";

std::string readString(yyjson_val* object, const char* key, const std::string& fallback) {
    return std::string { json::string(yyjson_obj_get(object, key), fallback) };
}

bool readBool(yyjson_val* object, const char* key, bool fallback) {
    return json::boolean(yyjson_obj_get(object, key), fallback);
}

void addString(yyjson_mut_doc* doc, yyjson_mut_val* object, const char* key, const std::string& value) {
    yyjson_mut_obj_add_strncpy(doc, object, key, value.data(), value.size());
}

}  // namespace

std::optional<Settings> load(const std::filesystem::path& file) {
    std::ifstream stream { file, std::ios::binary };
    if (!stream) return std::nullopt;
    const std::string content { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

    const auto doc = json::parse(content);
    auto* const root = json::root(doc);
    if (!yyjson_is_obj(root)) return std::nullopt;

    const Settings defaults;
    return Settings {
        .platformHostUrl = readString(root, platformHostUrl, defaults.platformHostUrl),
        .platformUsername = readString(root, platformUsername, defaults.platformUsername),
        .platformPassword = readString(root, platformPassword, defaults.platformPassword),
        .autoRunShout = readBool(root, autoRunShout, defaults.autoRunShout),
        .runAtStartup = readBool(root, runAtStartup, defaults.runAtStartup),
    };
}

bool save(const std::filesystem::path& file, const Settings& settings) {
    const auto doc = json::create();
    if (!doc) return false;

    auto* const root = yyjson_mut_obj(doc.get());
    yyjson_mut_doc_set_root(doc.get(), root);

    addString(doc.get(), root, platformHostUrl, settings.platformHostUrl);
    addString(doc.get(), root, platformUsername, settings.platformUsername);
    addString(doc.get(), root, platformPassword, settings.platformPassword);
    yyjson_mut_obj_add_bool(doc.get(), root, autoRunShout, settings.autoRunShout);
    yyjson_mut_obj_add_bool(doc.get(), root, runAtStartup, settings.runAtStartup);

    const auto text = json::write(doc, YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END);
    if (text.empty()) return false;

    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error) return false;

    // written aside, then renamed over the previous file : an interrupted write never leaves a truncated file
    auto temporary = file;
    temporary += ".tmp";

    std::ofstream stream { temporary, std::ios::binary | std::ios::trunc };
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();

    if (stream) std::filesystem::rename(temporary, file, error);
    if (!stream || error) {
        std::filesystem::remove(temporary, error);
        return false;
    }

    return true;
}

}  // namespace settings

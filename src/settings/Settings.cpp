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

#include <yyjson.h>

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>

namespace settings {

namespace {

constexpr const char * PLATFORM_HOST_URL = "platformHostUrl";
constexpr const char * PLATFORM_USERNAME = "platformUsername";
constexpr const char * PLATFORM_PASSWORD = "platformPassword";
constexpr const char * AUTO_RUN_SHOUT = "autoRunShout";
constexpr const char * RUN_AT_STARTUP = "runAtStartup";

using Document = std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)>;
using MutableDocument = std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)>;

std::string readString(yyjson_val * object, const char * key, const std::string& fallback) {
    const auto value = yyjson_obj_get(object, key);
    if (!yyjson_is_str(value)) return fallback;
    return { yyjson_get_str(value), yyjson_get_len(value) };
}

bool readBool(yyjson_val * object, const char * key, bool fallback) {
    const auto value = yyjson_obj_get(object, key);
    return yyjson_is_bool(value) ? yyjson_get_bool(value) : fallback;
}

void addString(yyjson_mut_doc * doc, yyjson_mut_val * object, const char * key, const std::string& value) {
    yyjson_mut_obj_add_strncpy(doc, object, key, value.data(), value.size());
}

}  // namespace

std::optional<Settings> load(const std::filesystem::path& file) {
    std::ifstream stream { file, std::ios::binary };
    if (!stream) return std::nullopt;
    const std::string content { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };

    const Document doc { yyjson_read(content.data(), content.size(), 0), &yyjson_doc_free };
    const auto root = yyjson_doc_get_root(doc.get());
    if (!yyjson_is_obj(root)) return std::nullopt;

    const Settings defaults;
    return Settings {
        .platformHostUrl = readString(root, PLATFORM_HOST_URL, defaults.platformHostUrl),
        .platformUsername = readString(root, PLATFORM_USERNAME, defaults.platformUsername),
        .platformPassword = readString(root, PLATFORM_PASSWORD, defaults.platformPassword),
        .autoRunShout = readBool(root, AUTO_RUN_SHOUT, defaults.autoRunShout),
        .runAtStartup = readBool(root, RUN_AT_STARTUP, defaults.runAtStartup),
    };
}

bool save(const std::filesystem::path& file, const Settings& settings) {
    const MutableDocument doc { yyjson_mut_doc_new(nullptr), &yyjson_mut_doc_free };
    if (!doc) return false;

    const auto root = yyjson_mut_obj(doc.get());
    yyjson_mut_doc_set_root(doc.get(), root);

    addString(doc.get(), root, PLATFORM_HOST_URL, settings.platformHostUrl);
    addString(doc.get(), root, PLATFORM_USERNAME, settings.platformUsername);
    addString(doc.get(), root, PLATFORM_PASSWORD, settings.platformPassword);
    yyjson_mut_obj_add_bool(doc.get(), root, AUTO_RUN_SHOUT, settings.autoRunShout);
    yyjson_mut_obj_add_bool(doc.get(), root, RUN_AT_STARTUP, settings.runAtStartup);

    std::size_t length = 0;
    const std::unique_ptr<char, decltype(&std::free)> json {
        yyjson_mut_write(doc.get(), YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END, &length),
        &std::free
    };
    if (!json) return false;

    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error) return false;

    // written aside, then renamed over the previous file : an interrupted write never leaves a truncated file
    auto temporary = file;
    temporary += ".tmp";

    std::ofstream stream { temporary, std::ios::binary | std::ios::trunc };
    stream.write(json.get(), static_cast<std::streamsize>(length));
    stream.close();

    if (stream) std::filesystem::rename(temporary, file, error);
    if (!stream || error) {
        std::filesystem::remove(temporary, error);
        return false;
    }

    return true;
}

}  // namespace settings

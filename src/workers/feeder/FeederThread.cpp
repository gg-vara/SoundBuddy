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

#include "FeederThread.h"

#include <exception>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <ITunesLibraryParser.hpp>

#include "src/helpers/Compression.h"
#include "src/helpers/Platform.h"
#include "src/helpers/UploadHelper.hpp"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/net/HttpClient.h"
#include "version.h"

FeederThread::FeederThread(AppSettings::ConnectivityInfos connectivityInfos, Callbacks callbacks) :
    _connectivityInfos(std::move(connectivityInfos)),
    _callbacks(std::move(callbacks)),
    _worker([this](std::stop_token stop) { _run(stop); }) {}

// the worker thread, destroyed first, is asked to stop (which aborts the upload in progress) and joined
FeederThread::~FeederThread() = default;

bool FeederThread::isRunning() const {
    return _running;
}

void FeederThread::_say(std::string text, workers::MessageKind kind) {
    workers::report(_callbacks.onMessage, "Feeder", { std::move(text), kind });
}

// runs on the worker thread
void FeederThread::_run(std::stop_token stop) {
    try {
        _feed(stop);
    } catch (const std::exception& e) {
        _say(e.what(), workers::MessageKind::Issue);
    }

    _running = false;
    if (_callbacks.onFinished) _callbacks.onFinished();
}

void FeederThread::_feed(std::stop_token stop) {
    const auto tr = [](std::string_view text) { return std::string { i18n::tr(text) }; };
    const auto musicApp = platform::musicAppName();

    _say(i18n::format(i18n::tr("WARNING ! Make sure you activated the XML file sharing in %1 > Preferences > Advanced."), { musicApp }));

    //
    _say(tr("Getting XML file location..."));

        // UTF-8 paths, as the library parser takes them
        const auto musicAppLibPath = platform::toUtf8(platform::musicLibraryFile());
        const auto outputPath = platform::toUtf8(AppSettings::getFeedOutputFilePath(false));
        const auto warningPath = platform::toUtf8(AppSettings::getFeedWarningFilePath());
        logs::info("Feeder", "Reading library {}", musicAppLibPath);

    //
    _say(tr("Collecting tracks infos..."));
        const ITunesXMLLibrary library { musicAppLibPath.c_str() };
    _say(tr("Collection done !"));

    //
    _say(tr("Parsing infos into JSON..."));

        const JSONLibrary json { library };
        logs::info("Feeder", "{} tracks, {} left out for missing metadata", json.tracksCount(), json.warningsCount());

        // if has missing fields tracks
        if (json.warningsCount()) {
            _say(i18n::format(
                i18n::tr("WARNING ! %1 files in your library are missing important "
                    "metadata and consequently were removed from the output file ! "
                    "Please check the \"%2\" file for more informations."),
                { std::to_string(json.warningsCount()), warningPath }
            ), workers::MessageKind::Warning);

            json.writeWarnings(warningPath.c_str());
        } else {
            // else remove warning file
            std::error_code error;
            std::filesystem::remove(AppSettings::getFeedWarningFilePath(), error);
        }

        //
        if (!json.tracksCount() && !json.warningsCount()) {
            _say(i18n::format(i18n::tr("No music found in your %1 library. Please feed it some."), { musicApp }), workers::MessageKind::Issue);
            return;
        }

        json.writeTracks(outputPath.c_str());

    _say(tr("Parsing done !"));
    _say(tr("OK, output file is ready for breakfast !"));

    if (_callbacks.onFilesGenerated) _callbacks.onFilesGenerated();
    if (stop.stop_requested()) return;

    _say(tr("Now we try to compress it..."));

        const auto outputJson = json.tracksJSON();
        const auto compressedData = compression::compress(outputJson);
        if (compressedData.empty()) {
            throw std::runtime_error("Library compression failed");
        }

        // write into file
        std::ofstream file { AppSettings::getFeedOutputFilePath(true), std::ios::binary | std::ios::trunc };
        file.write(compressedData.data(), static_cast<std::streamsize>(compressedData.size()));
        file.close();

    _say(i18n::format(i18n::tr("Went from %1 to %2"), { i18n::formatDataSize(outputJson.size()), i18n::formatDataSize(compressedData.size()) }));
    if (stop.stop_requested()) return;

    _say(tr("Let's try to send now !"));
        net::HttpClient client(APP_NAME "/" APP_CURRENT_VERSION);

    _say(tr("Sending... Waiting for response."));

        const auto response = UploadHelper::uploadDataToPlatform(client, {
            _connectivityInfos,
            AppSettings::getFeederUploadInfos(),
            compressedData,
        }, true, stop);

        if (stop.stop_requested()) return;

        if (!response.ok()) {
            _say(i18n::format(i18n::tr("An error occured while sending tracks infos to %1 platform : %2"),
                { DEST_PLATFORM_PRODUCT_NAME, UploadHelper::describeFailure(response) }), workers::MessageKind::Issue);
        } else if (!response.body.empty()) {
            logs::info("Feeder", "Uploaded, HTTP {}", response.status);
            _say(i18n::format(i18n::tr("Server responded: %1"), { response.body }));
        } else {
            _say(tr("No feedback from the server ? Strange... Please check the targeted host."), workers::MessageKind::Warning);
        }
}

// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FeederThread.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <system_error>
#include <utility>

#include <yyjson.h>

#include <ITunesXMLLibrary.hpp>
#include <JSONLibrary.hpp>

#include "src/helpers/AppSettings.h"
#include "src/helpers/Compression.h"
#include "src/helpers/Json.h"
#include "src/helpers/Platform.h"
#include "src/helpers/UploadHelper.h"
#include "src/i18n/Translations.h"
#include "src/logs/Logs.h"
#include "src/net/HttpClient.h"
#include "src/workers/base/Message.h"
#include "version.h"

FeederThread::FeederThread(AppSettings::ConnectivityInfos connectivityInfos, Callbacks callbacks) :
    _connectivityInfos(std::move(connectivityInfos)),
    _callbacks(std::move(callbacks)),
    _worker([this](const std::stop_token& stop) { _run(stop); }) {}

// the worker thread, destroyed first, is asked to stop (which aborts the upload in progress) and joined
FeederThread::~FeederThread() = default;

bool FeederThread::isRunning() const {
    return _running;
}

void FeederThread::_say(std::string text, workers::MessageKind kind) {
    workers::report(_callbacks.onMessage, "Feeder", { .text = std::move(text), .kind = kind });
}

// runs on the worker thread
void FeederThread::_run(const std::stop_token& stop) {
    try {
        _feed(stop);
    } catch (const std::exception& e) {
        _say(e.what(), workers::MessageKind::Issue);
    }

    _running = false;
    if (_callbacks.onFinished) _callbacks.onFinished();
}

void FeederThread::_feed(const std::stop_token& stop) {
    const auto tr = [](std::string_view text) { return std::string { i18n::tr(text) }; };
    const auto musicApp = platform::musicAppName();

    _say(i18n::format(i18n::tr("WARNING ! Make sure you activated the XML file sharing in %1 > Preferences > Advanced."), { musicApp }));

    //
    _say(tr("Getting XML file location..."));

    // UTF-8 paths, as the library parser takes them
    const auto musicAppLibPath = platform::toUtf8(platform::musicLibraryFile());
    const auto outputPath = platform::toUtf8(AppSettings::getFeedOutputFilePath(AppSettings::FeedFormat::Json));
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
    if (json.warningsCount() != 0U) {
        _say(
            i18n::format(
                i18n::tr(
                    "WARNING ! %1 files in your library are missing important "
                    "metadata and consequently were removed from the output file ! "
                    "Please check the \"%2\" file for more informations."
                ),
                { std::to_string(json.warningsCount()), warningPath }
            ),
            workers::MessageKind::Warning
        );

        json.writeWarnings(warningPath.c_str());
    } else {
        // else remove warning file
        std::error_code error;
        std::filesystem::remove(AppSettings::getFeedWarningFilePath(), error);
    }

    //
    if (json.tracksCount() == 0 && json.warningsCount() == 0) {
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
    std::ofstream file { AppSettings::getFeedOutputFilePath(AppSettings::FeedFormat::Compressed), std::ios::binary | std::ios::trunc };
    file.write(compressedData.data(), static_cast<std::streamsize>(compressedData.size()));
    file.close();

    _say(
        i18n::format(
            i18n::tr("Went from %1 to %2"), { i18n::formatDataSize(outputJson.size()), i18n::formatDataSize(compressedData.size()) }
        )
    );
    if (stop.stop_requested()) return;

    _say(tr("Let's try to send now !"));
    net::HttpClient client(APP_NAME "/" APP_CURRENT_VERSION);

    _say(tr("Sending... Waiting for response."));

    const auto response =
        UploadHelper::uploadLibrary(client, _connectivityInfos, compressedData, AppSettings::FeedFormat::Compressed, stop);

    if (stop.stop_requested()) return;

    if (!response.ok()) {
        _say(
            i18n::format(
                i18n::tr("An error occured while sending tracks infos to %1 platform : %2"),
                { DEST_PLATFORM_PRODUCT_NAME, UploadHelper::describeFailure(response) }
            ),
            workers::MessageKind::Issue
        );
    } else {
        logs::info("Feeder", "Uploaded, HTTP {}", response.status);
        const auto answer = json::parse(response.body);
        const auto changed = json::boolean(yyjson_obj_get(json::root(answer), "changed"), /*fallback=*/true);
        _say(tr(changed ? "Your library is online, bon appétit !" : "Your library online is already up to date."));
    }
}

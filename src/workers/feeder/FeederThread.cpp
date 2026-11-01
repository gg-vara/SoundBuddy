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

#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <exception>

#include <zlib.h>
#include <stdlib.h>

#include "src/helpers/Platform.h"
#include "src/helpers/UploadHelper.hpp"
#include "src/net/HttpClient.h"
#include "version.h"

#include <ITunesLibraryParser.hpp>

#include "src/i18n/trad.hpp"

FeederThread::FeederThread(const AppSettings::ConnectivityInfos connectivityInfos) : _connectivityInfos(connectivityInfos) {}

void FeederThread::run() {
    //
    emit forwardMessage(
        tr("WARNING ! Make sure you activated the XML file sharing in %1 > Preferences > Advanced.")
            .arg(musicAppName())
    );

    try {
        //
        emit forwardMessage(tr("Getting XML file location..."));

            // UTF-8 paths, as the library parser takes them
            const auto musicAppLibPath = platform::toUtf8(platform::musicLibraryFile());
            const auto outputPath = platform::toUtf8(AppSettings::getFeedOutputFilePath(false));
            const auto warningPath = platform::toUtf8(AppSettings::getFeedWarningFilePath());

        //
        emit forwardMessage(tr("Collecting tracks infos..."));
            const ITunesXMLLibrary library { musicAppLibPath.c_str() };
        emit forwardMessage(tr("Collection done !"));

        //
        emit forwardMessage(tr("Parsing infos into JSON..."));

            const JSONLibrary json { library };

            // if has missing fields tracks
            if(json.warningsCount()) {
                //
                emit forwardMessage(
                    tr("WARNING ! %1 files in your library are missing important "
                    "metadata and consequently were removed from the output file ! "
                    "Please check the \"%2\" file for more informations.")
                        .arg(json.warningsCount())
                        .arg(QString::fromStdString(warningPath)),
                    MessageType::WARNING
                );

                //
                json.writeWarnings(warningPath.c_str());
            } else {
                // else remove warning file
                std::error_code error;
                std::filesystem::remove(AppSettings::getFeedWarningFilePath(), error);
            }

            //
            if(!json.tracksCount() && !json.warningsCount()) {
                //
                emit forwardMessage(
                    tr("No music found in your %1 library. Please feed it some.")
                        .arg(musicAppName()),
                    MessageType::ISSUE
                );

                //
                return;
            }

            //
            json.writeTracks(outputPath.c_str());

        emit forwardMessage(tr("Parsing done !"));

        // send results
        emit forwardMessage(tr("OK, output file is ready for breakfast !"));

        emit filesGenerated();

        emit forwardMessage(tr("Now we try to compress it..."));

            const auto output_json = json.tracksJSON();
            auto compressedDataSize = compressBound(output_json.size()); // must be at least source len for starters, may be updated by zlib after sucessful compression
            auto compressedData = (unsigned char *)malloc(compressedDataSize);

            const auto compressionResult = compress(
                compressedData, 
                &compressedDataSize, 
                reinterpret_cast<const unsigned char *>(output_json.data()), 
                output_json.size()
            );

            if (compressionResult != Z_OK) {
                throw std::domain_error("zLib failed");
            }

            // write into file
            std::ofstream file { AppSettings::getFeedOutputFilePath(true), std::ios::binary | std::ios::trunc };
            file.write(reinterpret_cast<const char *>(compressedData), static_cast<std::streamsize>(compressedDataSize));
            file.close();

        emit forwardMessage(
            tr("Went from %1 to %2")
            .arg(QLocale::system().formattedDataSize(output_json.size()))
            .arg(QLocale::system().formattedDataSize(compressedDataSize))
        );

        emit forwardMessage(tr("Let's try to send now !"));
            //
            net::HttpClient client(APP_NAME "/" APP_CURRENT_VERSION);

        emit forwardMessage(tr("Sending... Waiting for response."));

            const auto uploadInfos = AppSettings::getFeederUploadInfos();
            const auto response = UploadHelper::uploadDataToPlatform(client, {
                this->_connectivityInfos,
                uploadInfos,
                std::string_view { reinterpret_cast<const char *>(compressedData), compressedDataSize },
            }, true);

            //
            if (!response.ok()) {
                emit forwardMessage(
                    tr("An error occured while sending tracks infos to %1 platform : %2")
                        .arg(DEST_PLATFORM_PRODUCT_NAME)
                        .arg(UploadHelper::describeFailure(response)),
                    MessageType::ISSUE
                );
            } else if (!response.body.empty()) {
                emit forwardMessage(
                    tr("Server responded: %1")
                        .arg(QString::fromStdString(response.body))
                );
            } else {
                emit forwardMessage(
                    tr("No feedback from the server ? Strange... Please check the targeted host."),
                    MessageType::WARNING
                );
            }

        // cleanup compressed data
        free(compressedData);

    //
    } catch (const std::exception& e) {
        // emit error as log
        emit forwardMessage(
            QString::fromUtf8(e.what()),
            MessageType::ISSUE
        );
    }
}

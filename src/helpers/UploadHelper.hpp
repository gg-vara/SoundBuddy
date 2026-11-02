#pragma once

#include <format>
#include <string>
#include <string_view>

#include "src/helpers/AppSettings.hpp"
#include "src/i18n/Translations.h"
#include "src/net/HttpClient.h"
#include "src/net/Multipart.h"

// uploads files to the platform, as multipart forms
class UploadHelper {
 public:
    struct UploadInstructions {
        const AppSettings::ConnectivityInfos &connectivityInfos;
        const AppSettings::UploadInfos &uploadInfos;
        std::string_view dataToUpload;

        std::string getUploadUrl() const {
            return connectivityInfos.getPlaformHomeUrl() + uploadInfos.path;
        }
    };

    // blocking
    static net::Response uploadDataToPlatform(net::HttpClient &client, const UploadInstructions &instructions, const bool isCompressed) {
        //
        const auto& fileName = instructions.uploadInfos.outputFileName;
        const auto fileExtension = AppSettings::getFeedOutputExtension(isCompressed);
        const auto contentType = AppSettings::getFeedOutputContentType(isCompressed);

        net::Multipart form;
        form.addFile(fileName, fileName + std::string { fileExtension }, std::string { contentType }, instructions.dataToUpload);

        // password...
        form.addField("password", instructions.connectivityInfos.password);

        // headless flag
        form.addField("headless", "1");

        // build request
        net::Request request { "POST", instructions.getUploadUrl() };
        request.headers.push_back({ "Accept-Language", std::string { i18n::languageCode() } });
        form.applyTo(request);

        return client.send(request);
    }

    // why an upload failed, for logs : transport error or HTTP status, followed by the platform explanations if any
    static std::string describeFailure(const net::Response &response) {
        auto description = response.error.empty()
            ? std::format("HTTP {}", response.status)
            : response.error;

        if (!response.body.empty()) {
            description += " >> " + response.body;
        }

        return description;
    }
};

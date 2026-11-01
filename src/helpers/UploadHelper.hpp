#pragma once

#include <QLocale>
#include <QString>

#include <string>
#include <string_view>

#include "src/helpers/AppSettings.hpp"
#include "src/net/HttpClient.h"
#include "src/net/Multipart.h"

// uploads files to the platform, as multipart forms
class UploadHelper {
 public:
    struct UploadInstructions {
        const AppSettings::ConnectivityInfos &connectivityInfos;
        const AppSettings::UploadInfos &uploadInfos;
        std::string_view dataToUpload;

        const std::string getUploadUrl() const {
            return (connectivityInfos.getPlaformHomeUrl() + uploadInfos.path).toStdString();
        }
    };

    // blocking
    static net::Response uploadDataToPlatform(net::HttpClient &client, const UploadInstructions &instructions, const bool isCompressed) {
        //
        const auto fileName = instructions.uploadInfos.outputFileName.toStdString();
        const auto fileExtension = AppSettings::getFeedOutputExtension(isCompressed).toStdString();
        const auto contentType = AppSettings::getFeedOutputContentType(isCompressed).toString().toStdString();

        net::Multipart form;
        form.addFile(fileName, fileName + fileExtension, contentType, instructions.dataToUpload);

        // password...
        form.addField("password", instructions.connectivityInfos.password.toStdString());

        // headless flag
        form.addField("headless", "1");

        // build request
        net::Request request { "POST", instructions.getUploadUrl() };
        request.headers.push_back({ "Accept-Language", QLocale::system().name().toStdString() });
        form.applyTo(request);

        return client.send(request);
    }

    // why an upload failed, for logs : transport error or HTTP status, followed by the platform explanations if any
    static QString describeFailure(const net::Response &response) {
        auto description = response.error.empty()
            ? QString("HTTP %1").arg(response.status)
            : QString::fromStdString(response.error);

        if (!response.body.empty()) {
            description += " >> " + QString::fromStdString(response.body);
        }

        return description;
    }
};

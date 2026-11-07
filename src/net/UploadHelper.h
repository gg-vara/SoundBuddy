// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <format>
#include <stop_token>
#include <string>
#include <string_view>

#include "src/settings/AppSettings.h"
#include "src/i18n/Translations.h"
#include "src/net/HttpClient.h"
#include "src/net/Multipart.h"
#include "src/net/PlatformSession.h"

// sends to the platform, with its API. Blocking; a stop requested on "stop" aborts the upload
class UploadHelper {
 public:
    // answers { "changed": false } when it is what is already there
    static net::Response uploadLibrary(
        net::HttpClient& client, const AppSettings::ConnectivityInfos& infos, std::string_view library, AppSettings::FeedFormat format,
        const std::stop_token& stop = {}
    ) {
        net::Multipart form;
        form.addFile(
            "file", "library" + std::string { AppSettings::getFeedOutputExtension(format) }, AppSettings::getFeedOutputContentType(format),
            library
        );

        auto request = _request("PUT", infos, "/api/v1/me/library");
        form.applyTo(request);

        return infos.session->send(client, request, stop);
    }

    static net::Response uploadShout(
        net::HttpClient& client, const AppSettings::ConnectivityInfos& infos, std::string_view shoutAsJson, const std::stop_token& stop = {}
    ) {
        auto request = _request("POST", infos, "/api/v1/me/shouts");
        request.headers.push_back({ .name = "Content-Type", .value = "application/json" });
        request.body = std::string { shoutAsJson };

        return infos.session->send(client, request, stop);
    }

    // why an upload failed, for logs : transport error or HTTP status, followed by the platform explanations if any
    static std::string describeFailure(const net::Response& response) {
        auto description = response.error.empty() ? std::format("HTTP {}", response.status) : response.error;

        const auto explanation = net::PlatformSession::explain(response);
        if (!explanation.empty()) {
            description += " >> " + explanation;
        }

        return description;
    }

 private:
    static net::Request _request(const char* method, const AppSettings::ConnectivityInfos& infos, std::string_view path) {
        net::Request request { .method = method, .url = infos.session->apiUrl() + std::string { path } };
        request.headers.push_back({ .name = "Accept-Language", .value = std::string { i18n::languageCode() } });
        return request;
    }
};

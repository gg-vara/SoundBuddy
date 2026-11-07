// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// AppSettings connectivity and upload infos, UploadHelper

#include "Check.h"
#include "TestSession.h"
#include "src/settings/AppSettings.h"
#include "src/net/UploadHelper.h"
#include "src/net/HttpClient.h"

int main() {
    const auto infos = [] {
        auto connected = connectedTo("https://host.example");
        connected.username = "j\xC3\xA9r\xC3\xB4me";
        return connected;
    }();
    CHECK(infos.getPlaformHomeUrl() == "https://host.example/u/j\xC3\xA9r\xC3\xB4me", "platform home URL");
    CHECK(AppSettings::ConnectivityInfos {}.getPlaformHomeUrl().empty(), "no URL without valid infos");

    CHECK(
        AppSettings::getFeedOutputExtension(AppSettings::FeedFormat::Compressed) == ".zmlib" &&
            AppSettings::getFeedOutputContentType(AppSettings::FeedFormat::Json) == "application/json" &&
            AppSettings::getFeedOutputContentType(AppSettings::FeedFormat::Compressed) == "application/octet-stream",
        "feed extension and content types"
    );

    net::Response refused;
    refused.status = 403;
    refused.body = "bad credentials";
    CHECK(UploadHelper::describeFailure(refused) == "HTTP 403 >> bad credentials", "failure : status, then platform explanations");

    refused.body = R"({"detail":"Not allowed"})";
    CHECK(UploadHelper::describeFailure(refused) == "HTTP 403 >> Not allowed", "failure : what the API explains");

    net::Response unreachable;
    unreachable.error = "connection refused";
    CHECK(UploadHelper::describeFailure(unreachable) == "connection refused", "failure : transport error");

    return check::summary();
}

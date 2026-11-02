// AppSettings connectivity and upload infos, UploadHelper

#include "Check.h"
#include "src/helpers/AppSettings.h"
#include "src/helpers/UploadHelper.h"
#include "src/net/HttpClient.h"

int main() {
    const AppSettings::ConnectivityInfos infos {
        .platformHostUrl = "https://host.example", .username = "j\xC3\xA9r\xC3\xB4me", .password = "pw", .areOK = true
    };
    CHECK(infos.getPlaformHomeUrl() == "https://host.example/u/j\xC3\xA9r\xC3\xB4me", "platform home URL");
    CHECK(infos.getSoundHeartbeatUrl() == "https://host.example/hb", "heartbeat URL");
    CHECK(AppSettings::ConnectivityInfos {}.getPlaformHomeUrl().empty(), "no URL without valid infos");

    const UploadHelper::UploadInstructions instructions { .connectivityInfos = infos,
                                                          .uploadInfos = AppSettings::getShoutUploadInfos(),
                                                          .dataToUpload = "{}" };
    CHECK(instructions.getUploadUrl() == "https://host.example/u/j\xC3\xA9r\xC3\xB4me/uploadShout", "shout upload URL");
    CHECK(AppSettings::getFeederUploadInfos().outputFileName == "SoundVitrine_file", "library upload file name");
    CHECK(
        AppSettings::getFeedOutputExtension(AppSettings::FeedFormat::Compressed) == ".zmlib" &&
            AppSettings::getFeedOutputContentType(AppSettings::FeedFormat::Json) == "application/json",
        "feed extension and content type"
    );

    net::Response refused;
    refused.status = 403;
    refused.body = "bad credentials";
    CHECK(UploadHelper::describeFailure(refused) == "HTTP 403 >> bad credentials", "failure : status, then platform explanations");

    net::Response unreachable;
    unreachable.error = "connection refused";
    CHECK(UploadHelper::describeFailure(unreachable) == "connection refused", "failure : transport error");

    return check::summary();
}

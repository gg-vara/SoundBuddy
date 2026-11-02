// FeederThread : a full run against an unreachable platform, and a stop during the upload.
// Reads the local iTunes / Music library when there is one; writes only to the tests data directory

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include "Check.h"
#include "TestData.h"
#include "src/helpers/AppSettings.h"
#include "src/helpers/Platform.h"
#include "src/i18n/Translations.h"
#include "src/workers/base/Message.h"
#include "src/workers/feeder/FeederThread.h"

using namespace std::chrono_literals;

namespace {

// what a feeder reports, from its thread
struct Report {
    std::mutex mutex;
    std::vector<workers::Message> messages;
    bool filesGenerated = false;
    bool finished = false;

    FeederThread::Callbacks callbacks() {
        return {
            .onMessage =
                [this](const workers::Message& message) {
                    const std::scoped_lock lock { mutex };
                    messages.push_back(message);
                },
            .onFilesGenerated =
                [this] {
                    const std::scoped_lock lock { mutex };
                    filesGenerated = true;
                },
            .onFinished =
                [this] {
                    const std::scoped_lock lock { mutex };
                    finished = true;
                },
        };
    }

    // waits until "predicate" holds, at most "timeout"
    template <typename Predicate>
    bool waitUntil(Predicate predicate, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                const std::scoped_lock lock { mutex };
                if (predicate()) return true;
            }
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }
};

long long millisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main() {
    bool hasLibrary = true;
    try {
        platform::musicLibraryFile();
    } catch (const std::exception&) {
        hasLibrary = false;
        std::printf("  no iTunes / Music library here : runs without it\n");
    }

    // full run, against a closed port
    {
        Report report;
        const FeederThread feeder { { .platformHostUrl = "http://127.0.0.1:9", .username = "user", .password = "password", .areOK = true },
                                    report.callbacks() };

        CHECK(report.waitUntil([&] { return report.finished; }, 60s), "run finished");
        CHECK(!feeder.isRunning(), "no longer running");

        const std::scoped_lock lock { report.mutex };
        CHECK(
            !report.messages.empty() && report.messages.back().kind == workers::MessageKind::Issue,
            "ends on an issue : no library, or platform unreachable"
        );
        if (hasLibrary) {
            CHECK(report.filesGenerated, "library export written");
            CHECK(
                std::filesystem::exists(AppSettings::getFeedOutputFilePath(AppSettings::FeedFormat::Compressed)),
                "compressed export written"
            );
            CHECK(report.messages.back().text.find("SoundVitrine") != std::string::npos, "upload failure reported");
        }
    }

    // stop during the upload, to an unroutable address (30 s timeout)
    if (hasLibrary) {
        Report report;
        auto start = std::chrono::steady_clock::now();
        {
            const FeederThread feeder {
                { .platformHostUrl = "http://10.255.255.1", .username = "user", .password = "password", .areOK = true }, report.callbacks()
            };
            const auto sending = std::string { i18n::tr("Sending... Waiting for response.") };
            CHECK(
                report.waitUntil([&] { return !report.messages.empty() && report.messages.back().text == sending; }, 60s), "upload started"
            );
            std::this_thread::sleep_for(200ms);
            start = std::chrono::steady_clock::now();
        }
        CHECK(millisecondsSince(start) < 1000, "stopped quickly during the upload");

        const std::scoped_lock lock { report.mutex };
        CHECK(
            report.finished && report.messages.back().kind != workers::MessageKind::Issue,
            "finished, an aborted upload is not reported as a failure"
        );
    }

    testdata::removeDataDirectory();
    return check::summary();
}

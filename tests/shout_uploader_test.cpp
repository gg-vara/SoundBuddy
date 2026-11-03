// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// ShoutUploader : failures reported, stopped quickly even during an upload

#include <chrono>
#include <cstdio>
#include <mutex>
#include <vector>

#include "Check.h"
#include "src/workers/base/Message.h"
#include "src/workers/shout/ShoutUploader.h"

using namespace std::chrono_literals;

namespace {

// collects the messages of a worker, from its thread
struct Messages {
    std::mutex mutex;
    std::vector<workers::Message> received;

    workers::MessageSink sink() {
        return [this](const workers::Message& message) {
            const std::scoped_lock lock { mutex };
            received.push_back(message);
        };
    }

    std::vector<workers::Message> get() {
        const std::scoped_lock lock { mutex };
        return received;
    }
};

long long millisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main() {
    // refused connection : the failure is reported, then the queue is drained
    {
        Messages messages;
        ShoutUploader uploader { { .platformHostUrl = "http://127.0.0.1:9", .username = "user", .password = "password", .areOK = true },
                                 messages.sink() };

        uploader.upload("{}");
        uploader.upload("{}");
        CHECK(uploader.waitForDrain(10s), "queue drained");

        const auto received = messages.get();
        CHECK(received.size() == 2, "one message per failed upload");
        CHECK(
            !received.empty() && received[0].kind == workers::MessageKind::Issue && received[0].line == workers::LineMode::Append,
            "reported as an issue"
        );
        CHECK(!received.empty() && received[0].text.find("SoundVitrine") != std::string::npos, "message names the platform");
    }

    // unroutable address : the connection would wait for the timeout (30 s), destruction must not
    {
        Messages messages;
        auto start = std::chrono::steady_clock::now();
        {
            ShoutUploader uploader {
                { .platformHostUrl = "http://10.255.255.1", .username = "user", .password = "password", .areOK = true }, messages.sink()
            };
            uploader.upload("{}");
            uploader.upload("{}");
            CHECK(!uploader.waitForDrain(300ms), "upload still in progress");
            start = std::chrono::steady_clock::now();
        }
        const auto elapsed = millisecondsSince(start);
        std::printf("  stopped in %lld ms\n", elapsed);
        CHECK(elapsed < 2000, "destruction aborts the upload in progress quickly");
        CHECK(messages.get().empty(), "an aborted upload is not reported as a failure");
    }

    // nothing queued : destruction right away
    {
        const auto start = std::chrono::steady_clock::now();
        {
            const ShoutUploader uploader { {}, {} };
        }
        CHECK(millisecondsSince(start) < 500, "idle uploader stopped right away");
    }

    return check::summary();
}

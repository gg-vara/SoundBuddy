// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// HeartbeatThread : statuses, stopped quickly while waiting or requesting

#include <chrono>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

#include "Check.h"
#include "TestSession.h"
#include "src/workers/heartbeat/HeartbeatThread.h"

using namespace std::chrono_literals;

namespace {

// collects the statuses of a heartbeat, from its thread
struct Statuses {
    std::mutex mutex;
    std::vector<heartbeat::Status> received;

    heartbeat::OnStatus sink() {
        return [this](const heartbeat::Status& status) {
            const std::scoped_lock lock { mutex };
            received.push_back(status);
        };
    }

    std::vector<heartbeat::Status> get() {
        const std::scoped_lock lock { mutex };
        return received;
    }

    // waits for "count" statuses at most "timeout"
    bool waitFor(std::size_t count, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (get().size() >= count) return true;
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
    // no credentials : refused right away, then the thread ends
    {
        Statuses statuses;
        const HeartbeatThread heartbeat { {}, statuses.sink() };
        CHECK(statuses.waitFor(1, 2s), "status without credentials");
        const auto received = statuses.get();
        CHECK(received.size() == 1 && received[0].connectivity == heartbeat::Connectivity::Refused, "refused for lack of credentials");
    }

    // platform unreachable : checking, refused, then waits for the next heartbeat (30 s), destruction must not
    {
        Statuses statuses;
        auto start = std::chrono::steady_clock::now();
        {
            const HeartbeatThread heartbeat { connectedTo("http://127.0.0.1:9"), statuses.sink() };
            CHECK(statuses.waitFor(2, 10s), "two statuses");
            start = std::chrono::steady_clock::now();
        }
        CHECK(millisecondsSince(start) < 500, "stopped right away while waiting for the next heartbeat");

        const auto received = statuses.get();
        CHECK(
            received.size() == 2 && received[0].connectivity == heartbeat::Connectivity::Checking &&
                received[1].connectivity == heartbeat::Connectivity::Refused,
            "checking, then refused"
        );
        CHECK(received.size() == 2 && received[1].text.find("SoundVitrine") != std::string::npos, "refusal names the platform");
    }

    // unroutable address : the request would wait for its timeout (3 s), destruction must not
    {
        Statuses statuses;
        auto start = std::chrono::steady_clock::now();
        {
            const HeartbeatThread heartbeat { connectedTo("http://10.255.255.1"), statuses.sink() };
            CHECK(statuses.waitFor(1, 2s), "checking");
            std::this_thread::sleep_for(300ms);
            start = std::chrono::steady_clock::now();
        }
        CHECK(millisecondsSince(start) < 1000, "stopped quickly during a request");
        CHECK(statuses.get().size() == 1, "an aborted check reports nothing more");
    }

    return check::summary();
}

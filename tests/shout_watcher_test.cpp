// ShoutWatcher : waits for the music app, stopped right away. Skipped while the music app runs,
// not to connect to the user's session

#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#endif

#include "Check.h"
#include "src/workers/shout/ShoutWatcher.h"

using namespace std::chrono_literals;

namespace {

long long millisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

int main() {
#ifdef _WIN32
    if (FindWindowW(nullptr, L"iTunes")) {
        std::printf("  SKIP  iTunes is running\n");
        return check::summary();
    }
#else
    std::printf("  SKIP  the macOS watcher queries Music right away\n");
    return check::summary();
#endif

    std::mutex mutex;
    std::vector<workers::Message> messages;
    std::vector<std::string> shouts;
    bool finished = false;

    auto start = std::chrono::steady_clock::now();
    {
        const ShoutWatcher watcher { {
            .onMessage = [&](const workers::Message& message) { const std::scoped_lock lock { mutex }; messages.push_back(message); },
            .onShout = [&](std::string shout) { const std::scoped_lock lock { mutex }; shouts.push_back(std::move(shout)); },
            .onFinished = [&] { const std::scoped_lock lock { mutex }; finished = true; },
        } };

        // waits for the music app, polled every second
        std::this_thread::sleep_for(1500ms);
        CHECK(watcher.isRunning(), "running while waiting for the music app");
        start = std::chrono::steady_clock::now();
    }
    CHECK(millisecondsSince(start) < 500, "stopped right away while waiting");

    const std::scoped_lock lock { mutex };
    CHECK(messages.size() == 1 && messages[0].text.find("iTunes") != std::string::npos, "waiting message, naming the music app");
    CHECK(shouts.empty(), "nothing shouted without a session");
    CHECK(finished, "finished callback");

    return check::summary();
}

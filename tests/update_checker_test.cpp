// UpdateChecker : outside of a Velopack installation, one check at a time, not waited for on destruction

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Check.h"
#include "src/workers/base/UpdateChecker.h"

using namespace std::chrono_literals;

int main() {
    std::mutex mutex;
    std::condition_variable checked;
    bool done = false;
    UpdateChecker::CheckResults results;

    {
        UpdateChecker checker;
        CHECK(
            checker.check([&](const UpdateChecker::CheckResults& r) {
                const std::scoped_lock lock { mutex };
                results = r;
                done = true;
                checked.notify_all();
            }),
            "check started"
        );

        CHECK(!checker.check({}) || done, "a second check is refused while the first one runs");

        std::unique_lock lock { mutex };
        CHECK(checked.wait_for(lock, 30s, [&] { return done; }), "check finished");
        CHECK(results.result == UpdateChecker::Result::NotInstalled, "not installed : this test is no Velopack installation");
        std::printf("  %s\n", results.error.c_str());
    }

    // destroyed while checking : neither waits nor calls back
    {
        bool calledBack = false;
        const auto start = std::chrono::steady_clock::now();
        {
            UpdateChecker checker;
            checker.check([&](const UpdateChecker::CheckResults&) { calledBack = true; });
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        CHECK(elapsed < 1000, "destruction does not wait for the check");

        // the abandoned check ends on its own
        std::this_thread::sleep_for(2s);
        CHECK(!calledBack, "an abandoned check does not call back");
    }

    return check::summary();
}

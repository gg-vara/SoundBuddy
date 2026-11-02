// SoundBuddy tests : minimal checks, each test being a program whose exit code is its failure count

#pragma once

#include <cstdio>

namespace check {

inline int failures = 0;

inline void report(bool ok, const char * what, const char * file, int line) {
    if (ok) {
        std::printf("  OK    %s\n", what);
    } else {
        std::printf("  FAIL  %s (%s:%d)\n", what, file, line);
        ++failures;
    }
}

// to return from main()
inline int summary() {
    std::printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASSED", failures);
    std::fflush(stdout);
    return failures;
}

}  // namespace check

#define CHECK(condition, what) ::check::report(static_cast<bool>(condition), what, __FILE__, __LINE__)

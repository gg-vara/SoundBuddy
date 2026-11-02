// src/helpers/Timestamps.h

#include <regex>

#include "Check.h"
#include "src/helpers/Timestamps.h"

int main() {
    CHECK(std::regex_match(timestamps::utcISO(), std::regex(R"(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ)")), "UTC, ISO 8601");
    CHECK(std::regex_match(timestamps::local(), std::regex(R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)")), "local time");
    CHECK(
        std::regex_match(timestamps::localPrecise(), std::regex(R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3})")), "local time with milliseconds"
    );

    return check::summary();
}

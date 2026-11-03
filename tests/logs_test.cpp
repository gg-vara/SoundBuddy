// SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// src/logs : format, concurrent writes, rotation, restart

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "Check.h"
#include "TestData.h"
#include "src/logs/Logs.h"

namespace {

std::vector<std::string> readLines(const std::filesystem::path& file) {
    std::ifstream stream { file, std::ios::binary };
    std::vector<std::string> lines;
    for (std::string line; std::getline(stream, line);) lines.push_back(line);
    return lines;
}

}  // namespace

int main() {
    const auto directory = testdata::scratchDirectory("logs \u00e9") / "logs";
    const auto file = directory / "soundbuddy.log";
    const auto rotated = directory / "soundbuddy.1.log";

    logs::info("Test", "before start");  // debugger output only
    logs::start(file);
    CHECK(std::filesystem::is_directory(directory), "directory created");

    logs::info("Test", "caf\xC3\xA9 {} \xE9\x9F\xB3", 42);
    logs::debug("Test", "debug line");

#ifdef _DEBUG
    constexpr std::size_t expectedLines = 2;
#else
    constexpr std::size_t expectedLines = 1;
#endif
    const auto first = readLines(file);
    CHECK(first.size() == expectedLines, "line before start left out, debug line in debug builds only");
    CHECK(
        !first.empty() && std::regex_match(first[0], std::regex(R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3} \[INFO\] \[Test\] café 42 音)")),
        "line format, UTF-8"
    );

    // 4 threads x 5000 lines of about 90 bytes : 1.8 MB, so one rotation
    std::vector<std::thread> threads;
    threads.reserve(4);
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([t] {
            for (int i = 0; i < 5000; ++i) logs::warning("Thread", "thread {} line {:05} padding-padding-padding", t, i);
        });
    }
    for (auto& thread : threads) thread.join();

    CHECK(std::filesystem::exists(rotated), "rotated into soundbuddy.1.log");
    CHECK(
        std::filesystem::file_size(file) <= std::uintmax_t { 1024 } * 1024 &&
            std::filesystem::file_size(rotated) <= std::uintmax_t { 1024 } * 1024,
        "files under 1 MB"
    );

    auto all = readLines(rotated);
    const auto current = readLines(file);
    all.insert(all.end(), current.begin(), current.end());

    const std::regex threadLine(
        R"(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3} \[WARNING\] \[Thread\] thread \d line \d{5} padding-padding-padding)"
    );
    std::size_t threadLines = 0;
    std::size_t broken = 0;
    for (const auto& line : all) {
        if (line.find("[Thread]") == std::string::npos) continue;
        ++threadLines;
        if (!std::regex_match(line, threadLine)) ++broken;
    }
    CHECK(threadLines == 20000 && broken == 0, "every concurrent line written whole, none lost");

    const auto before = readLines(file).size();
    logs::start(file);
    logs::error("Test", "after restart");
    CHECK(readLines(file).size() == before + 1, "restart appends to the existing file");

    logs::stop();
    logs::info("Test", "after stop");
    CHECK(readLines(file).size() == before + 1, "nothing written after stop");

    testdata::removeScratchDirectories();
    return check::summary();
}

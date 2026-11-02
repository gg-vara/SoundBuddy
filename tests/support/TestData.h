// SoundBuddy tests : directories the tests write to, removed only after checking they belong to the tests

#pragma once

#include <filesystem>
#include <string_view>
#include <system_error>

#include "src/helpers/Platform.h"
#include "version.h"

namespace testdata {

// temporary directory of a test, created empty : <temp>/SoundBuddyTests/<name> ("name" in UTF-8)
inline std::filesystem::path scratchDirectory(std::string_view name) {
    const auto directory = std::filesystem::temp_directory_path() / APP_NAME / platform::fromUtf8(name);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    std::filesystem::create_directories(directory, error);
    return directory;
}

// removes <temp>/SoundBuddyTests
inline void removeScratchDirectories() {
    const auto directory = std::filesystem::temp_directory_path() / APP_NAME;
    if (directory.filename() != APP_NAME) return;

    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

// removes the data directory of the tests (<data>/SoundBuddyTestsOrg), never the app one
inline void removeDataDirectory() {
    const auto directory = platform::dataDirectory();
    if (directory.filename() != APP_NAME || directory.parent_path().filename() != APP_PUBLISHER_ORG) return;

    std::error_code error;
    std::filesystem::remove_all(directory.parent_path(), error);
}

}  // namespace testdata

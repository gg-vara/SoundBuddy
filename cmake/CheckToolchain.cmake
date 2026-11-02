# Compiler requirements, checked with explicit messages (see prerequisites/ and README.md)

# C++20 library features used by the app (std::format, <chrono> formatting, std::jthread) : LLVM 18 or newer
SET(SOUNDBUDDY_MIN_LLVM_VERSION 18)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    message(WARNING "Apple Clang found (${CMAKE_CXX_COMPILER}) instead of Homebrew's LLVM Clang : see README.md")
elseif(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "LLVM Clang is expected, found ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER}) : see README.md")
elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS SOUNDBUDDY_MIN_LLVM_VERSION)
    message(FATAL_ERROR "LLVM Clang ${SOUNDBUDDY_MIN_LLVM_VERSION} or newer is expected, found ${CMAKE_CXX_COMPILER_VERSION} : update the toolchain (pacman -Syu, or brew upgrade llvm)")
endif()

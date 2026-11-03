# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# macOS toolchain : Homebrew's LLVM (prerequisites/Brewfile), against the SDK of Xcode or of its Command Line Tools

# macOS 13.3 or newer : std::format of floating-point numbers needs std::to_chars of the system libc++, from 13.3
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.3)

# Homebrew's LLVM, rather than Apple's Clang, which comes first in the PATH
execute_process(COMMAND brew --prefix llvm OUTPUT_VARIABLE LLVM_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT LLVM_PREFIX)
    set(LLVM_PREFIX /opt/homebrew/opt/llvm)
endif()
if(NOT EXISTS ${LLVM_PREFIX}/bin/clang++)
    message(FATAL_ERROR "Homebrew's LLVM not found in ${LLVM_PREFIX} : install the build prerequisites (see prerequisites/Brewfile)")
endif()

set(CMAKE_CXX_COMPILER ${LLVM_PREFIX}/bin/clang++)
set(CMAKE_OBJCXX_COMPILER ${LLVM_PREFIX}/bin/clang++)
set(CMAKE_AR ${LLVM_PREFIX}/bin/llvm-ar)
set(CMAKE_RANLIB ${LLVM_PREFIX}/bin/llvm-ranlib)
set(CMAKE_NM ${LLVM_PREFIX}/bin/llvm-nm)
list(APPEND CMAKE_PREFIX_PATH ${LLVM_PREFIX})

# the system's frameworks and libraries, which Homebrew's LLVM does not find by itself
execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE OSX_SDK_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_OSX_SYSROOT ${OSX_SDK_PATH})

# Apple's linker, rather than lld : it builds the stubs Clang emits for Objective-C messages (objc_msgSendClass$...),
# which lld does not, and signs ad hoc

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-O0")  # added to CMake's -g
set(CMAKE_OBJCXX_FLAGS_DEBUG_INIT "-O0")

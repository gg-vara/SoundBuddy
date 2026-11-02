# SoundBuddy : configured from CMakePresets.json, with the toolchain files of toolchains/ (see README.md)
cmake_minimum_required(VERSION 3.25)

set(GITHUB_REPO_OWNER "gg-vara")
set(GITHUB_REPO_NAME "SoundBuddy")

# the version of a release is bumped here (see README.md)
project(${GITHUB_REPO_NAME} VERSION 0.11.0
    DESCRIPTION "Small companion app that feeds SoundVitrine with iTunes / Music library metadata"
    HOMEPAGE_URL https://github.com/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

#################
# Configuration #
#################

# unit tests, run by ctest (see tests/CMakeLists.txt)
option(SOUNDBUDDY_BUILD_TESTS "Build the unit tests" OFF)
# signs the Windows packages (see cmake/Velopack.cmake) : disabled for local development builds
option(SOUNDBUDDY_CODESIGN "Sign the Windows packages" ON)
# clang-tidy while compiling the app and its tests, its warnings failing the build (see .clang-tidy, and the CI)
option(SOUNDBUDDY_LINT "Lint the sources while compiling them" OFF)

if(SOUNDBUDDY_BUILD_TESTS)
    enable_testing()
endif()

# compiler requirements
include(CheckToolchain)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compiler cache, if installed
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Using ccache : ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

############
# Identity #
############

# written into the generated version.h (templates/_version.h)
set(APP_PUBLISHER_ORG "LVNWL")  # the organization of the executables' signing certificate
set(APP_PUBLISHER_ORG_PKG_NORM "lvnwl")  # as a bundle identifier part : lowercase, no spaces
set(DEST_PLATFORM_PRODUCT_NAME "SoundVitrine")
set(APP_PATCHNOTE_URL ${PROJECT_HOMEPAGE_URL}/releases)
# GitHub repository whose releases (published with Velopack, see cmake/Velopack.cmake) feed updates
set(APP_UPDATES_REPOSITORY_URL ${PROJECT_HOMEPAGE_URL})

configure_file(${PROJECT_SOURCE_DIR}/templates/_version.h ${PROJECT_BINARY_DIR}/generated/version.h)

################
# Dependencies #
################

# JSON
include(yyjson)

# music library export
add_subdirectory(deps/ItunesLibraryParser)

# core sources, shared with the tests
include(SoundBuddyCore)

# the targets defined from here on are linted, the dependencies above are not
if(SOUNDBUDDY_LINT)
    find_program(CLANG_TIDY_EXECUTABLE clang-tidy REQUIRED)
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE} --quiet --warnings-as-errors=*)
    set(CMAKE_OBJCXX_CLANG_TIDY ${CMAKE_CXX_CLANG_TIDY})
endif()

#######
# App #
#######

add_executable(appExec
    src/main.cpp
    src/ui/Controller.cpp
    src/workers/base/UpdateChecker.cpp
)
soundbuddy_add_core(appExec PRIVATE)

target_include_directories(appExec PRIVATE ${PROJECT_BINARY_DIR}/generated)
set_target_properties(appExec PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    # Velopack packages this folder
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/out
)

if(WIN32)
    configure_file(${PROJECT_SOURCE_DIR}/templates/win/version.rc ${PROJECT_BINARY_DIR}/version.rc @ONLY)
    target_sources(appExec PRIVATE
        src/ui/win/MainWindow.win.cpp
        # icons, manifest; executable metadata
        resources/resources.rc
        ${PROJECT_BINARY_DIR}/version.rc
    )

    target_link_libraries(appExec PRIVATE
        # window, webview host
        dwmapi comctl32 shlwapi
    )
    # C++ libraries linked in : the executable depends on system libraries and Velopack's only
    target_link_options(appExec PRIVATE -static)

    # without console in release builds
    set_target_properties(appExec PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

    # debug builds : DWARF moves to SoundBuddy.exe.debug, next to the executable, which links to it (.gnu_debuglink) :
    # gdb and lldb find it there. Neither reads split DWARF (.dwo) and PDB both
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(TARGET appExec POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE_NAME:appExec> $<TARGET_FILE_NAME:appExec>.debug
            COMMAND ${CMAKE_OBJCOPY} --strip-debug --add-gnu-debuglink=$<TARGET_FILE_NAME:appExec>.debug $<TARGET_FILE_NAME:appExec>
            WORKING_DIRECTORY $<TARGET_FILE_DIR:appExec>
            COMMENT "Moving debug info to $<TARGET_FILE_NAME:appExec>.debug"
            VERBATIM
        )
    endif()
elseif(APPLE)
    enable_language(OBJCXX)

    set(APP_ICON ${PROJECT_SOURCE_DIR}/resources/icons/app.icns)
    # menu bar icon, a template image
    set(APP_MENU_BAR_ICON ${PROJECT_SOURCE_DIR}/resources/icons/app_reverse.png)
    set_source_files_properties(${APP_ICON} ${APP_MENU_BAR_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    target_sources(appExec PRIVATE
        src/ui/mac/MainWindow.mac.mm
        ${APP_ICON}
        ${APP_MENU_BAR_ICON}
    )

    set_target_properties(appExec PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/templates/osx/Info.plist
        MACOSX_BUNDLE_ICON_FILE app.icns
        MACOSX_BUNDLE_GUI_IDENTIFIER com.${APP_PUBLISHER_ORG_PKG_NORM}.${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )

    # debug symbols
    add_custom_command(TARGET appExec POST_BUILD
        COMMAND xcrun dsymutil $<TARGET_FILE:appExec> -o $<TARGET_BUNDLE_DIR:appExec>/../$<TARGET_FILE_NAME:appExec>.dSYM
    )
endif()

# a page in the system webview (see cmake/webview.cmake) : WebView2 on Windows, WKWebView on macOS
include(webview)
target_link_libraries(appExec PRIVATE webview::core)

# installer, auto-updates, and velopack_pack / velopack_publish packaging targets, which also sign (and notarize, on macOS)
include(Velopack)

#########
# Tests #
#########

if(SOUNDBUDDY_BUILD_TESTS)
    add_subdirectory(tests)
endif()

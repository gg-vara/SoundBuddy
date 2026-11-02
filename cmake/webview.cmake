# User interface : a web page (src/ui/page) shown in the system webview, through webview/webview.
#
# - webview::core : header-only C++ API; on Windows, WebView2 headers are fetched and its loader is built in,
#   the WebView2 runtime itself being part of the system
# - generated/page.h : the page, built by Bun and Vite into a single HTML file, as a C++ string (see templates/page.h.in)
# - "page" and "page-lint" tests : the page's own tests (bun test) and its lint (Biome), with the unit tests

include(FetchContent)

SET(WEBVIEW_VERSION 0.12.0)

SET(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
SET(WEBVIEW_BUILD_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
SET(WEBVIEW_ENABLE_CHECKS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(webview
    URL https://github.com/webview/webview/archive/refs/tags/${WEBVIEW_VERSION}.tar.gz
    URL_HASH SHA256=e2c8d0bed3fcd13e624074448b7d3b1d0b0d03546a15839f17ee0013f2cba1db
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    # its headers are left out of warnings and lint
    SYSTEM
)
FetchContent_MakeAvailable(webview)

########
# Page #
########

# Bun : JavaScript runtime and package manager, running Vite and TypeScript
# installed by its script, or by winget
file(TO_CMAKE_PATH "$ENV{USERPROFILE}" USER_PROFILE_DIR)
file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" LOCAL_APP_DATA_DIR)
find_program(BUN_EXECUTABLE bun
    HINTS
        ${USER_PROFILE_DIR}/.bun/bin
        ${LOCAL_APP_DATA_DIR}/Microsoft/WinGet/Links
    # a tool of the system, out of the toolchain's root
    NO_CMAKE_FIND_ROOT_PATH
)
if(NOT BUN_EXECUTABLE)
    message(FATAL_ERROR "Bun not found : it builds the user interface page (see README.md). Install it, or set BUN_EXECUTABLE to its path")
endif()

SET(PAGE_DIR ${PROJECT_SOURCE_DIR}/src/ui/page)
SET(PAGE_BUILD_DIR ${CMAKE_BINARY_DIR}/page)
SET(PAGE_HEADER ${CMAKE_BINARY_DIR}/generated/page.h)

file(GLOB_RECURSE PAGE_INPUTS CONFIGURE_DEPENDS ${PAGE_DIR}/src/*)
list(APPEND PAGE_INPUTS
    ${PAGE_DIR}/index.html
    ${PAGE_DIR}/package.json
    ${PAGE_DIR}/tsconfig.json
    ${PAGE_DIR}/vite.config.ts
)
if(EXISTS ${PAGE_DIR}/bun.lock)
    list(APPEND PAGE_INPUTS ${PAGE_DIR}/bun.lock)
endif()

add_custom_command(
    OUTPUT ${PAGE_HEADER}
    # dependencies, as locked
    COMMAND ${BUN_EXECUTABLE} install --frozen-lockfile
    # types, then the single HTML file
    COMMAND ${BUN_EXECUTABLE} x --bun tsc --noEmit
    COMMAND ${BUN_EXECUTABLE} x --bun vite build --outDir ${PAGE_BUILD_DIR} --emptyOutDir
    COMMAND ${CMAKE_COMMAND}
        -DPAGE_HTML_FILE=${PAGE_BUILD_DIR}/index.html
        -DTEMPLATE=${PROJECT_SOURCE_DIR}/templates/page.h.in
        -DOUTPUT=${PAGE_HEADER}
        -P ${PROJECT_SOURCE_DIR}/cmake/EmbedPage.cmake
    WORKING_DIRECTORY ${PAGE_DIR}
    DEPENDS ${PAGE_INPUTS} ${PROJECT_SOURCE_DIR}/templates/page.h.in ${PROJECT_SOURCE_DIR}/cmake/EmbedPage.cmake
    COMMENT "Building the user interface page (Bun, Vite)..."
    VERBATIM
)

# built before the sources including it
target_sources(appExec PRIVATE ${PAGE_HEADER})

if(SOUNDBUDDY_BUILD_TESTS)
    add_test(NAME page COMMAND ${BUN_EXECUTABLE} test WORKING_DIRECTORY ${PAGE_DIR})
    # lint and format, as checked by Biome (biome.json) : "bun run format" fixes what it can
    add_test(NAME page-lint COMMAND ${BUN_EXECUTABLE} x biome ci . WORKING_DIRECTORY ${PAGE_DIR})
endif()

# https://stackoverflow.com/a/51444436/3021058
MESSAGE(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")

# policies first
if(COMMAND cmake_policy)
    # cmake_policy(SET CMP0011 NEW)
    # cmake_policy(SET CMP0020 NEW)
    # cmake_policy(SET CMP0003 NEW)
endif()

# use ccache
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message("== Using CCache ! ==")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PROGRAM}")
endif()

# default configuration
list(APPEND CMAKE_MODULE_PATH 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Qt
)

if(WIN32)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/win)
endif()

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)

#############
## STARTUP ##
#############

cmake_minimum_required(VERSION 3.19)

SET(GITHUB_REPO_OWNER "gg-vara")
SET(GITHUB_REPO_NAME "SoundBuddy")

### PLEASE LET VERSION NEXT TO PROJECT NAME !!! since cmake has no documented way to do this, We use grep to ack project version on this specific line 
### please make sure that project name is without whitespaces !!!
project(${GITHUB_REPO_NAME} VERSION 0.11.0
    DESCRIPTION "Small companion app that feeds SoundVitrine with iTunes / Music library metadata"
    HOMEPAGE_URL https://github.com/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}
    LANGUAGES CXX
)

# https://cmake.org/cmake/help/v3.2/module/CPackBundle.html#variable:CPACK_BUNDLE_APPLE_CERT_APP
# https://github.com/ziglang/zig/issues/6971
# Since recent SDK, if not signed, any executable will silently crash
if(APPLE)
    if(NOT DEFINED ENV{"APPLE_CODESIGN_ID"})
        message(FATAL_ERROR "APPLE_CODESIGN_ID is expected as env variable. Please check README.md for details.")
    endif()

    if(NOT DEFINED ENV{"APPLE_CERT_APP_CODE_SIGN_IDENTITY"})
    message(FATAL_ERROR "APPLE_CERT_APP_CODE_SIGN_IDENTITY is expected as env variable. Please check README.md for details.")
    endif()

    SET(APPLE_CODESIGN_ID ENV{"APPLE_CODESIGN_ID"})
    SET(APPLE_CERT_APP_CODE_SIGN_IDENTITY ENV{"APPLE_CERT_APP_CODE_SIGN_IDENTITY"})
endif()

########################
## VERSION.H HANDLING ##
########################

SET(APP_PUBLISHER_ORG "LVNWL") # Should always be the same as the org set in executable signer certificate
SET(APP_PUBLISHER_ORG_PKG_NORM "lvnwl") # please make sure that publisher has no whitespaces and in lowercase !!!
SET(DEST_PLATFORM_PRODUCT_NAME "SoundVitrine")
SET(APP_PATCHNOTE_URL ${PROJECT_HOMEPAGE_URL}/releases)

# GitHub repository whose releases (published with Velopack, see cmake/Velopack.cmake) feed updates
SET(APP_UPDATES_REPOSITORY_URL ${PROJECT_HOMEPAGE_URL})


################################
## CPP Compiler Configuration ##
################################

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_C_STANDARD 11)

# Qt
SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets
    Network
    LinguistTools
)

#
qt_standard_project_setup(
    I18N_SOURCE_LANGUAGE en
    I18N_TRANSLATED_LANGUAGES fr
)

if(APPLE)
    find_package(Qt6 REQUIRED COMPONENTS 
        DBus
    )
endif()

##########################
## Bundle configuration ##
##########################

SET(BUNDLE_TYPE "")

if (WIN32)
    #set bundle type for release only (bypasses log into terminal)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        SET(BUNDLE_TYPE WIN32)
    endif()

    #add icon
    SET(APP_ICO ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.rc) 
endif()

if (APPLE)
    # enforce searching for OpenSSL from explicit Homebrew path
    find_package(OpenSSL REQUIRED)
    #set bundle type
    SET(BUNDLE_TYPE MACOSX_BUNDLE) 
    #add icons
    SET(MACOSX_BUNDLE_ICON_FILE "app.icns")
    SET(APP_ICO ${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/${MACOSX_BUNDLE_ICON_FILE})
    set_source_files_properties(${APP_ICO} PROPERTIES 
        MACOSX_PACKAGE_LOCATION "Resources"                                                    
    )
endif()

####################
# Code compilation #
####################

file(GLOB_RECURSE APP_CPP_FILES "src/*.cpp")
file(GLOB_RECURSE APP_HPP_FILES "src/*.hpp")

if (APPLE)
    list(REMOVE_ITEM APP_CPP_FILES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shout/win/iTunesCOMClient.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shout/win/iTunesSession.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shout/ShoutWatcher.win.cpp
    )
elseif (WIN32)
    list(REMOVE_ITEM APP_CPP_FILES 
        ${CMAKE_CURRENT_SOURCE_DIR}/src/workers/shout/ShoutWatcher.osx.cpp
    )
endif()

#########################
# Executable Generation #
#########################

# Create executable
add_executable(appExec ${BUNDLE_TYPE}
    ${APP_HPP_FILES}
    ${APP_CPP_FILES}
    ${APP_ICO}
    resources/resources.qrc
) 

# generate debug symbols
if (APPLE)
    add_custom_command(TARGET appExec
        COMMAND xcrun dsymutil $<TARGET_FILE:appExec> -o $<TARGET_BUNDLE_DIR:appExec>/../$<TARGET_FILE_NAME:appExec>.dSYM
    )
endif()

# add executable metadata for Windows .exe
if (WIN32)
    #
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/templates/win/version.rc
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
    @ONLY)

    #
    target_sources(appExec PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

# can be disabled for local development builds, which do not need to be signed
option(SOUNDBUDDY_CODESIGN "Sign Windows executables" ON)

#
if (WIN32 AND SOUNDBUDDY_CODESIGN)
    find_package(signtool REQUIRED)
    
    SET(EXPECTED_PFX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/certs/all.pfx)

    if(NOT EXISTS "${EXPECTED_PFX_PATH}")
        message(FATAL_ERROR ".pfx file is expected to be generated first on Windows targets to sign executables and installers. Please run toolchains/win_generate_pfx.sh in its folder first !")
    endif()

    SET(TIMESTAMP_SERVER "http://timestamp.acs.microsoft.com")

    add_custom_command(TARGET appExec
        POST_BUILD
        COMMAND ${signtool_EXECUTABLE} sign /f "${EXPECTED_PFX_PATH}" /tr "${TIMESTAMP_SERVER}" /td SHA256 /fd SHA256 "$<TARGET_FILE:appExec>"
        VERBATIM
    )
endif()

#
target_include_directories(appExec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

#
set_target_properties(appExec PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})

#
target_compile_definitions(appExec PRIVATE $<$<CONFIG:Debug>:_DEBUG>) #define _DEBUG markup if in DEBUG config

if (APPLE)
    #add highdpi support / menu behavior via plist template (MACOS)
    set_target_properties(appExec PROPERTIES 
        BUNDLE                              ON
        MACOSX_BUNDLE_INFO_PLIST            ${CMAKE_CURRENT_SOURCE_DIR}/templates/osx/Info.plist
        MACOSX_BUNDLE_GUI_IDENTIFIER        com.${APP_PUBLISHER_ORG_PKG_NORM}.${CMAKE_PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_NAME           ${CMAKE_PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION        ${CMAKE_PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING  ${CMAKE_PROJECT_VERSION} 
    ) 
endif()

################
# translations #
################

include(QtTranslation)

# app sources
HandleQtTranslation(
    TARGET appExec
    INSTALL_COMPONENT "App"
    OUTPUT_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/app_qm
    SOURCES 
        ${APP_CPP_FILES}
        ${APP_HPP_FILES}
    TRANSLATION_FILES
        "src/i18n/app_fr.ts"
)

add_dependencies(appExec update_translations)

############
# Velopack #
############

# installer, auto-updates, and velopack_pack / velopack_publish packaging targets
include(Velopack)

#######################
# ITunesLibraryParser #
#######################

add_subdirectory(deps/ITunesLibraryParser)

target_link_libraries(appExec PRIVATE
    itunes-library-parser
)

########
# Zlib #
########

find_package(ZLIB REQUIRED)

target_link_libraries(appExec PRIVATE
    ZLIB::ZLIB
)

#
#
#

find_package(PkgConfig)

# only the libraries used by DASH streaming (libpostproc no longer exists since FFmpeg 8)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswresample
)

target_link_libraries(appExec PRIVATE PkgConfig::ffmpeg)


########
# QT 6 #
########

target_link_libraries(appExec PRIVATE
    Qt6::Core
    Qt6::Gui 
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    # iTunes COM API
    target_link_libraries(appExec PRIVATE
        ole32
        oleaut32
        uuid
    )
endif()

if(APPLE)
    target_link_libraries(appExec PRIVATE
        Qt6::DBus
    )
endif()

# QT LIBRARIES COPY
include(DeployQt)
DeployQt(appExec)


if(WIN32)
    include(CMakeDependencies)
    DeployPEDependencies(appExec "AppRuntime")
endif()

###########
# Version #
###########

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/templates/_version.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
)

#
install(TARGETS appExec
    COMPONENT "App"
    BUNDLE DESTINATION "."
)

if(WIN32)
    install(FILES ${VELOPACK_LIB}
        RENAME velopack_libc.dll
        TYPE BIN
        COMPONENT "AppRuntime"
    )
endif()
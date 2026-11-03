# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Runs Velopack's CLI, in CMake script mode : see velopack_pack / velopack_publish targets in cmake/Velopack.cmake
#
# ACTION=pack : fetches the previous GitHub release (for delta updates), then packages PACK_DIR into RELEASES_DIR
# ACTION=publish : uploads RELEASES_DIR as the vPACK_VERSION GitHub release, published unless
#   SOUNDBUDDY_RELEASE_DRAFT environment variable is set
#
# The GitHub token comes from GITHUB_TOKEN environment variable, else from GitHub CLI (gh auth token)

cmake_minimum_required(VERSION 3.25)

#
# checks
#

if(NOT DOTNET)
    message(FATAL_ERROR ".NET SDK not found : it runs Velopack's CLI (see README.md)")
endif()

# CLI of the tools manifest (.config/dotnet-tools.json), found from the working directory
set(VPK ${DOTNET} tool run vpk --)

execute_process(COMMAND ${VPK} -h OUTPUT_VARIABLE vpk_help ERROR_QUIET)
if(NOT vpk_help MATCHES "Velopack CLI ${VPK_VERSION}")
    message(FATAL_ERROR "Velopack CLI ${VPK_VERSION} is not available : run \"dotnet tool restore\" from the repository")
endif()

set(github_args --repoUrl ${REPOSITORY_URL} --channel ${CHANNEL} --outputDir ${RELEASES_DIR})

# GitHub token, only needed to publish (the repository is public) : never written to the cache
# nor to command lines displayed by the build
function(get_github_token out)
    set(token "$ENV{GITHUB_TOKEN}")

    if(NOT token)
        find_program(GH gh)
        if(GH)
            # when not logged in, gh may still print a message on its output, with an error code
            execute_process(COMMAND ${GH} auth token
                OUTPUT_VARIABLE token
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE gh_result
                ERROR_QUIET
            )
            if(NOT gh_result EQUAL 0)
                set(token "")
            endif()
        endif()
    endif()

    set(${out} "${token}" PARENT_SCOPE)
endfunction()

#
# pack
#

if(ACTION STREQUAL "pack")
    file(REMOVE_RECURSE ${RELEASES_DIR})

    # previous release, to generate delta updates (none for the very first release)
    execute_process(COMMAND ${VPK} download github ${github_args} RESULT_VARIABLE download_result)
    if(NOT download_result EQUAL 0)
        message(STATUS "No previous release downloaded : only full packages will be generated")
    endif()

    set(pack_args
        --packId ${PACK_ID}
        --packVersion ${PACK_VERSION}
        --packTitle ${PACK_TITLE}
        --packAuthors ${PACK_AUTHORS}
        --packDir ${PACK_DIR}
        --mainExe ${MAIN_EXE}
        --runtime ${RUNTIME}
        --channel ${CHANNEL}
        --icon ${ICON}
        --outputDir ${RELEASES_DIR}
    )

    if(SIGN_PARAMS)
        list(APPEND pack_args --signParams ${SIGN_PARAMS})
    endif()

    if(SIGN_APP_IDENTITY)
        list(APPEND pack_args --signAppIdentity ${SIGN_APP_IDENTITY})
    endif()

    if(SIGN_INSTALL_IDENTITY)
        list(APPEND pack_args --signInstallIdentity ${SIGN_INSTALL_IDENTITY})
    endif()

    if(NOTARY_PROFILE)
        list(APPEND pack_args --notaryProfile ${NOTARY_PROFILE})
    endif()

    # runtimes the installer installs when missing
    if(FRAMEWORK)
        list(APPEND pack_args --framework ${FRAMEWORK})
    endif()

    execute_process(COMMAND ${VPK} pack ${pack_args} COMMAND_ERROR_IS_FATAL ANY)

    message(STATUS "Packages ready in ${RELEASES_DIR}")

#
# publish
#

elseif(ACTION STREQUAL "publish")
    if(NOT EXISTS ${RELEASES_DIR})
        message(FATAL_ERROR "Nothing to publish in ${RELEASES_DIR} : build velopack_pack target first")
    endif()

    get_github_token(token)
    if(NOT token)
        message(FATAL_ERROR "No GitHub token : set GITHUB_TOKEN environment variable, or log in with GitHub CLI (gh auth login)")
    endif()

    # the other platform may already have created the release
    set(upload_args
        --token ${token}
        --tag v${PACK_VERSION}
        --releaseName "${PACK_ID} v${PACK_VERSION}"
        --merge
    )

    if(NOT "$ENV{SOUNDBUDDY_RELEASE_DRAFT}")
        list(APPEND upload_args --publish)
    endif()

    execute_process(COMMAND ${VPK} upload github ${github_args} ${upload_args} COMMAND_ERROR_IS_FATAL ANY)

    message(STATUS "Published ${PACK_ID} v${PACK_VERSION} (${CHANNEL}) to ${REPOSITORY_URL}")

else()
    message(FATAL_ERROR "Unknown ACTION '${ACTION}', expected pack or publish")
endif()

# Velopack : installer and auto-updates, through GitHub releases.
#
# - links the Velopack C / C++ library to appExec
# - velopack_pack target : packages the built app (installer, full and delta update packages) into ${CMAKE_BINARY_DIR}/releases
# - velopack_publish target : uploads these packages as the vX.Y.Z GitHub release
#
# Packaging targets require Velopack's CLI, pinned to the same version as the library by the
# .NET tools manifest (.config/dotnet-tools.json) : install it once with "dotnet tool restore"

SET(VELOPACK_VERSION 1.2.161)

# the CLI and the library must stay on the same version
file(READ ${CMAKE_SOURCE_DIR}/.config/dotnet-tools.json DOTNET_TOOLS_MANIFEST)
string(JSON VPK_MANIFEST_VERSION GET ${DOTNET_TOOLS_MANIFEST} tools vpk version)
if(NOT VPK_MANIFEST_VERSION STREQUAL VELOPACK_VERSION)
    message(FATAL_ERROR "Velopack versions differ : ${VELOPACK_VERSION} in cmake/Velopack.cmake, ${VPK_MANIFEST_VERSION} in .config/dotnet-tools.json")
endif()

###########
# Library #
###########

# prebuilt library with a C ABI, so the MSVC build can be linked from MinGW / clang
include(FetchContent)
FetchContent_Declare(velopack
    URL https://github.com/velopack/velopack/releases/download/${VELOPACK_VERSION}/velopack_libc_${VELOPACK_VERSION}.zip
    URL_HASH SHA256=31de8f2cf05a47bd5328a9f98b0c41aec1582156f6075f1058b26941f7f17c20
    DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(velopack)

add_library(velopack SHARED IMPORTED)
set_target_properties(velopack PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${velopack_SOURCE_DIR}/include
)

if(WIN32)
    SET(VELOPACK_LIB ${velopack_SOURCE_DIR}/lib/velopack_libc_win_x64_msvc.dll)
    set_target_properties(velopack PROPERTIES
        IMPORTED_LOCATION ${VELOPACK_LIB}
        IMPORTED_IMPLIB ${VELOPACK_LIB}.lib
    )

    # the import library refers to "velopack_libc.dll"
    add_custom_command(TARGET appExec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VELOPACK_LIB} $<TARGET_FILE_DIR:appExec>/velopack_libc.dll
    )
elseif(APPLE)
    SET(VELOPACK_LIB ${velopack_SOURCE_DIR}/lib/velopack_libc_osx.dylib)
    set_target_properties(velopack PROPERTIES
        IMPORTED_LOCATION ${VELOPACK_LIB}
    )

    # the library id is an absolute path from Velopack's CI : load it from the bundle instead
    add_custom_command(TARGET appExec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VELOPACK_LIB} $<TARGET_FILE_DIR:appExec>/libvelopack_libc.dylib
        COMMAND install_name_tool -id @executable_path/libvelopack_libc.dylib $<TARGET_FILE_DIR:appExec>/libvelopack_libc.dylib
        COMMAND install_name_tool -change /Users/runner/_work/velopack/velopack/target/x86_64-apple-darwin/release/deps/libvelopack_libc.dylib
            @executable_path/libvelopack_libc.dylib $<TARGET_FILE:appExec>
        # these changes void the ad hoc signatures, without which Apple Silicon Macs refuse to run them : signed again, ad hoc.
        # Velopack signs the packages for real (see below)
        COMMAND codesign --force --sign - $<TARGET_FILE_DIR:appExec>/libvelopack_libc.dylib
        COMMAND codesign --force --sign - $<TARGET_FILE:appExec>
    )
endif()

target_link_libraries(appExec PRIVATE
    velopack
)

#############
# Packaging #
#############

# runs the CLI of the tools manifest
find_program(DOTNET_EXECUTABLE dotnet
    # default install locations, when not on PATH (as from MSYS2 shells)
    HINTS "$ENV{ProgramFiles}/dotnet" "C:/Program Files/dotnet" "/usr/local/share/dotnet" "/opt/homebrew/bin"
)
if(NOT DOTNET_EXECUTABLE)
    message(STATUS ".NET SDK not found : velopack_pack and velopack_publish targets will not work (see README.md)")
endif()

# arguments of cmake/velopack/vpk.cmake, common to packaging and publishing
SET(VELOPACK_SCRIPT_ARGS
    -DDOTNET=${DOTNET_EXECUTABLE}
    -DVPK_VERSION=${VELOPACK_VERSION}
    -DRELEASES_DIR=${CMAKE_BINARY_DIR}/releases
    -DREPOSITORY_URL=${APP_UPDATES_REPOSITORY_URL}
    -DPACK_ID=${PROJECT_NAME}
    -DPACK_VERSION=${PROJECT_VERSION}
)

if(WIN32)
    list(APPEND VELOPACK_SCRIPT_ARGS
        -DCHANNEL=win
        -DRUNTIME=win-x64
        -DPACK_DIR=$<TARGET_FILE_DIR:appExec>
        -DICON=${CMAKE_SOURCE_DIR}/resources/icons/package.ico
        # the user interface is a page in WebView2, part of Windows 10 and 11 : installed if missing
        -DFRAMEWORK=webview2
    )

    # Velopack signs the executable, the updater and the installer (with the signtool it brings), with certs/all.pfx
    if(SOUNDBUDDY_CODESIGN)
        set(EXPECTED_PFX_PATH ${PROJECT_SOURCE_DIR}/certs/all.pfx)
        if(NOT EXISTS "${EXPECTED_PFX_PATH}")
            message(FATAL_ERROR ".pfx file is expected to sign the packages : run toolchains/win_generate_pfx.sh in its folder first, or configure without SOUNDBUDDY_CODESIGN")
        endif()

        list(APPEND VELOPACK_SCRIPT_ARGS
            "-DSIGN_PARAMS=/f ${EXPECTED_PFX_PATH} /tr http://timestamp.acs.microsoft.com /td SHA256 /fd SHA256"
        )
    endif()
elseif(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        SET(VELOPACK_RUNTIME osx-arm64)
    else()
        SET(VELOPACK_RUNTIME osx-x64)
    endif()

    list(APPEND VELOPACK_SCRIPT_ARGS
        -DCHANNEL=osx
        -DRUNTIME=${VELOPACK_RUNTIME}
        -DPACK_DIR=$<TARGET_BUNDLE_DIR:appExec>
        -DICON=${CMAKE_SOURCE_DIR}/resources/icons/package.icns
        # Velopack signs the bundle and the installer, then has them notarized, as these variables tell (see README.md) :
        # left out, the packages are not signed, which Gatekeeper refuses
        "-DSIGN_APP_IDENTITY=$ENV{APPLE_CERT_APP_CODE_SIGN_IDENTITY}"
        "-DSIGN_INSTALL_IDENTITY=$ENV{APPLE_CERT_INSTALL_SIGN_IDENTITY}"
        "-DNOTARY_PROFILE=$ENV{APPLE_NOTARY_PROFILE}"
    )
endif()

add_custom_target(velopack_pack
    COMMAND ${CMAKE_COMMAND}
        ${VELOPACK_SCRIPT_ARGS}
        -DACTION=pack
        -DMAIN_EXE=$<TARGET_FILE_NAME:appExec>
        -DPACK_TITLE=${PROJECT_NAME}
        -DPACK_AUTHORS=${APP_PUBLISHER_ORG}
        -P ${CMAKE_SOURCE_DIR}/cmake/velopack/vpk.cmake
    DEPENDS appExec
    # where .NET finds the tools manifest
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Packaging ${PROJECT_NAME} ${PROJECT_VERSION} with Velopack..."
    VERBATIM
)

# publishes what velopack_pack produced
add_custom_target(velopack_publish
    COMMAND ${CMAKE_COMMAND}
        ${VELOPACK_SCRIPT_ARGS}
        -DACTION=publish
        -P ${CMAKE_SOURCE_DIR}/cmake/velopack/vpk.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Publishing ${PROJECT_NAME} v${PROJECT_VERSION} on GitHub..."
    VERBATIM
)

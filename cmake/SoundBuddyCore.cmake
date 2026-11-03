# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# The sources of SoundBuddy, apart from its entry point and user interface, and what they need.
#
# The app and the tests' core library compile them each, as each brings its own version.h : the generated one for the
# app, tests/support/version.h for the tests, which then keep to their own data directory, registry values and locks.

set(SOUNDBUDDY_CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../src)
cmake_path(NORMAL_PATH SOUNDBUDDY_CORE_DIR)

# adds the core sources to "target", their requirements being PUBLIC or PRIVATE ("scope") to its users
function(soundbuddy_add_core target scope)
    set(src ${SOUNDBUDDY_CORE_DIR})

    target_sources(${target} PRIVATE
        ${src}/helpers/AppSettings.cpp
        ${src}/helpers/Hash.cpp
        ${src}/helpers/Platform.cpp
        ${src}/i18n/Translations.cpp
        ${src}/logs/Logs.cpp
        ${src}/net/HttpClient.cpp
        ${src}/net/SseParser.cpp
        ${src}/settings/Settings.cpp
        ${src}/workers/base/Message.cpp
        ${src}/workers/feeder/FeederThread.cpp
        ${src}/workers/mBeat/MBeatThread.cpp
        ${src}/workers/shout/ShoutPayload.cpp
        ${src}/workers/shout/ShoutPayloadConsumer.cpp
        ${src}/workers/shout/ShoutUploader.cpp
        ${src}/workers/shout/ShoutWatcher.cpp
    )

    # "src/..." includes
    target_include_directories(${target} ${scope} ${SOUNDBUDDY_CORE_DIR}/..)
    target_compile_definitions(${target} ${scope} $<$<CONFIG:Debug>:_DEBUG>)
    target_link_libraries(${target} ${scope} yyjson itunes-library-parser)

    if(WIN32)
        target_sources(${target} PRIVATE
            ${src}/helpers/AppSettings.win.cpp
            ${src}/helpers/Compression.win.cpp
            ${src}/helpers/Platform.win.cpp
            ${src}/net/HttpClient.win.cpp
            ${src}/workers/shout/ShoutWatcher.win.cpp
            ${src}/workers/shout/win/iTunesCOMClient.cpp
            ${src}/workers/shout/win/iTunesSession.cpp
        )
        target_compile_definitions(${target} ${scope} UNICODE _UNICODE)
        target_link_libraries(${target} ${scope}
            # iTunes COM API
            ole32 oleaut32 uuid
            # HTTP client
            winhttp
            # file hashes
            bcrypt
            # library compression (Compression API, MSZIP)
            cabinet
            # platform services : shell, registry
            shell32 advapi32
        )
    elseif(APPLE)
        target_sources(${target} PRIVATE
            ${src}/helpers/AppSettings.mac.cpp
            ${src}/helpers/Compression.mac.cpp
            ${src}/helpers/Platform.mac.mm
            ${src}/net/HttpClient.mac.mm
            ${src}/workers/shout/ShoutWatcher.mac.cpp
        )
        target_compile_options(${target} ${scope} $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
        target_link_libraries(${target} ${scope}
            # settings of previous versions
            "-framework CoreFoundation"
            # HTTP client
            "-framework Foundation"
            # platform services
            "-framework AppKit"
            # library compression (libz of the macOS SDK)
            z
        )
    endif()
endfunction()

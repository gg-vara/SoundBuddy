# SoundBuddy tests, run by ctest : see the *-debug test presets, or the windows-test workflow.
#
# Each test is a program built from the sources it covers. tests/support/version.h replaces the
# generated one, so that tests use their own data directory, registry values and locks.

get_filename_component(TOOLCHAIN_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

###############
# Tested core #
###############

# app sources without user interface, shared by the tests
add_library(soundbuddy_tested_core STATIC
    ${PROJECT_SOURCE_DIR}/src/helpers/Hash.cpp
    ${PROJECT_SOURCE_DIR}/src/helpers/Platform.cpp
    ${PROJECT_SOURCE_DIR}/src/i18n/Translations.cpp
    ${PROJECT_SOURCE_DIR}/src/logs/Logs.cpp
    ${PROJECT_SOURCE_DIR}/src/net/HttpClient.cpp
    ${PROJECT_SOURCE_DIR}/src/settings/Settings.cpp
    ${PROJECT_SOURCE_DIR}/src/workers/base/Message.cpp
    $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/helpers/Compression.win.cpp>
    $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/helpers/Platform.win.cpp>
    $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/net/HttpClient.win.cpp>
    $<$<PLATFORM_ID:Darwin>:${PROJECT_SOURCE_DIR}/src/helpers/Compression.mac.cpp>
    $<$<PLATFORM_ID:Darwin>:${PROJECT_SOURCE_DIR}/src/helpers/Platform.mac.mm>
    $<$<PLATFORM_ID:Darwin>:${PROJECT_SOURCE_DIR}/src/net/HttpClient.mac.mm>
)

target_include_directories(soundbuddy_tested_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/support  # first : version.h of the tests
    ${PROJECT_SOURCE_DIR}
)

target_compile_definitions(soundbuddy_tested_core PUBLIC $<$<CONFIG:Debug>:_DEBUG>)
target_link_libraries(soundbuddy_tested_core PUBLIC yyjson)

if(WIN32)
    target_compile_definitions(soundbuddy_tested_core PUBLIC UNICODE _UNICODE)
    target_link_libraries(soundbuddy_tested_core PUBLIC bcrypt cabinet shell32 ole32 uuid advapi32 winhttp)
elseif(APPLE)
    target_compile_options(soundbuddy_tested_core PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    target_link_libraries(soundbuddy_tested_core PUBLIC "-framework Foundation" "-framework AppKit" z)
endif()

set_target_properties(soundbuddy_tested_core PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

#########
# Tests #
#########

# test "name", built from <name>_test.cpp and SOURCES, linked to LIBRARIES; LABELS tag it (as "network")
function(soundbuddy_test name)
    cmake_parse_arguments(TEST "" "" "SOURCES;LIBRARIES;LABELS" ${ARGN})

    add_executable(test_${name} ${name}_test.cpp ${TEST_SOURCES})
    target_link_libraries(test_${name} PRIVATE soundbuddy_tested_core ${TEST_LIBRARIES})

    # out of the app directory, which is packaged
    set_target_properties(test_${name} PROPERTIES
        AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    if(APPLE)
        target_compile_options(test_${name} PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
    endif()

    add_test(NAME ${name} COMMAND test_${name})

    # toolchain libraries (libc++, Qt) lie next to the compiler
    if(WIN32)
        set_tests_properties(${name} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${TOOLCHAIN_BIN_DIR}")
    endif()

    if(TEST_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${TEST_LABELS}")
    endif()
endfunction()

soundbuddy_test(timestamps)
soundbuddy_test(hash)
soundbuddy_test(compression)
soundbuddy_test(json)
soundbuddy_test(logs)
soundbuddy_test(platform)

soundbuddy_test(upload)

# Qt : translation bridge, and import of the settings of previous versions
soundbuddy_test(i18n LIBRARIES Qt6::Core)
soundbuddy_test(settings SOURCES ${PROJECT_SOURCE_DIR}/src/helpers/AppSettings.cpp LIBRARIES Qt6::Core)

soundbuddy_test(shout SOURCES
    ${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutPayload.cpp
    ${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutPayloadConsumer.cpp
)

soundbuddy_test(sse SOURCES ${PROJECT_SOURCE_DIR}/src/net/SseParser.cpp)

# workers, against unreachable platforms : local closed port, unroutable address
soundbuddy_test(shout_uploader SOURCES ${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutUploader.cpp)
soundbuddy_test(mbeat SOURCES ${PROJECT_SOURCE_DIR}/src/workers/mBeat/MBeatThread.cpp)
soundbuddy_test(feeder SOURCES ${PROJECT_SOURCE_DIR}/src/workers/feeder/FeederThread.cpp LIBRARIES itunes-library-parser)
soundbuddy_test(shout_watcher
    SOURCES
        ${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutPayload.cpp
        ${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutPayloadConsumer.cpp
        ${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutWatcher.cpp
        $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutWatcher.win.cpp>
        $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/workers/shout/win/iTunesCOMClient.cpp>
        $<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/workers/shout/win/iTunesSession.cpp>
        $<$<PLATFORM_ID:Darwin>:${PROJECT_SOURCE_DIR}/src/workers/shout/ShoutWatcher.osx.cpp>
    LIBRARIES
        $<$<PLATFORM_ID:Windows>:oleaut32>
        $<$<PLATFORM_ID:Darwin>:Qt6::Core>
)

# against public test services (httpbin.org, stream.wikimedia.org) : left out of the default test runs
soundbuddy_test(network
    SOURCES ${PROJECT_SOURCE_DIR}/src/net/SseParser.cpp
    LABELS network
)

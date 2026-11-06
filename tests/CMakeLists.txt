# SoundBuddy tests, run by ctest : see the *-debug test presets, or the windows-test workflow.
#
# Each test is a program linked to the tests' core library : the app's core sources (cmake/SoundBuddyCore.cmake)
# compiled with tests/support/version.h, so that tests use their own data directory, registry values and locks.

get_filename_component(TOOLCHAIN_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)

###############
# Tested core #
###############

add_library(soundbuddy_tested_core STATIC)
soundbuddy_add_core(soundbuddy_tested_core PUBLIC)

# first : version.h of the tests
target_include_directories(soundbuddy_tested_core BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)

#########
# Tests #
#########

# test "name", built from <name>_test.cpp and SOURCES, linked to LIBRARIES, run with ARGS; LABELS tag it (as "network")
function(soundbuddy_test name)
    cmake_parse_arguments(TEST "" "" "SOURCES;LIBRARIES;LABELS;ARGS" ${ARGN})

    add_executable(test_${name} ${name}_test.cpp ${TEST_SOURCES})
    target_link_libraries(test_${name} PRIVATE soundbuddy_tested_core ${TEST_LIBRARIES})

    # out of the app directory, which is packaged
    set_target_properties(test_${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    add_test(NAME ${name} COMMAND test_${name} ${TEST_ARGS})

    # toolchain libraries (libc++) lie next to the compiler
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
soundbuddy_test(session)
soundbuddy_test(i18n)
# import of the settings of previous versions, as QSettings stored them : in the registry on Windows, the preferences on macOS
soundbuddy_test(settings)
soundbuddy_test(shout)
soundbuddy_test(sse)

# workers, against unreachable platforms : local closed port, unroutable address
soundbuddy_test(shout_uploader)
soundbuddy_test(mbeat)
soundbuddy_test(feeder)
soundbuddy_test(shout_watcher)

# messages between the application and its page, checked against the page's types : the test writes them as TypeScript,
# which tsc checks, with the page's compiler options (src/ui/page/tsconfig.json)
set(PAGE_CONTRACT_DIR ${CMAKE_CURRENT_BINARY_DIR}/page_contract)
file(CONFIGURE OUTPUT ${PAGE_CONTRACT_DIR}/tsconfig.json @ONLY CONTENT [=[
{
  "extends": "@PAGE_DIR@/tsconfig.json",
  "compilerOptions": { "types": [] },
  "files": ["contract.ts"],
  "include": []
}
]=])
soundbuddy_test(page_contract ARGS ${PAGE_CONTRACT_DIR}/contract.ts)
target_compile_definitions(test_page_contract PRIVATE PAGE_SOURCE_DIR="${PAGE_DIR}/src")
set_tests_properties(page_contract PROPERTIES FIXTURES_SETUP page_contract)
add_test(NAME page-contract-types COMMAND ${BUN_EXECUTABLE} x --bun tsc -p ${PAGE_CONTRACT_DIR}/tsconfig.json WORKING_DIRECTORY ${PAGE_DIR})
set_tests_properties(page-contract-types PROPERTIES FIXTURES_REQUIRED page_contract)

# updates, through Velopack, which only the app links otherwise
soundbuddy_test(update_checker SOURCES ${PROJECT_SOURCE_DIR}/src/workers/base/UpdateChecker.cpp LIBRARIES velopack)
if(WIN32)
    # the import library refers to "velopack_libc.dll" (see cmake/Velopack.cmake)
    add_custom_command(TARGET test_update_checker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${VELOPACK_LIB} $<TARGET_FILE_DIR:test_update_checker>/velopack_libc.dll
    )
endif()

# what keeps a run of the app away from the session's real data
soundbuddy_test(isolation)

# the real executable, without a window, against a local server standing in for SoundVitrine
add_test(NAME headless COMMAND ${BUN_EXECUTABLE} test ./headless.test.ts WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_tests_properties(headless PROPERTIES ENVIRONMENT
    "SOUNDBUDDY_DEBUG_BUILD=$<CONFIG:Debug>;SOUNDBUDDY_EXE=$<TARGET_FILE:appExec>;SOUNDBUDDY_LIBRARY=${PROJECT_SOURCE_DIR}/deps/ItunesLibraryParser/tests/samples/iTunes Music Library (S).xml"
)

# against public test services (httpbin.org, stream.wikimedia.org) : left out of the default test runs
soundbuddy_test(network LABELS network)

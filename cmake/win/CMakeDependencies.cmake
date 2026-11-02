if(NOT CMakeDependencies_INCLUDED)

###########
# pe-util #
###########

if(MINGW)
    # set by toolchains/msys2.cmake
    if(NOT MSYS2_CLANG64_ROOT)
        message(FATAL_ERROR "MSYS2_CLANG64_ROOT must be set ! Please use a toolchain file !")
    endif()

    # use different separator to allow injection of whitelisted elements
    string(REPLACE ";" "|" PEUTIL_WHITELIST_EXTENSIONS_SAFE "${PEUTIL_WHITELIST_EXTENSIONS}")

    #pe-util is required to find all .dll dependencies
    include(ExternalProject)
    ExternalProject_Add(peldd
        GIT_REPOSITORY  "https://github.com/Amphaal/pe-util.git"
        INSTALL_COMMAND ""
        CMAKE_ARGS -DPEUTIL_DEFAULT_SEARCH_PATH=${MSYS2_CLANG64_ROOT}/bin
                   -DPEUTIL_WHITELIST_EXTENSIONS=${PEUTIL_WHITELIST_EXTENSIONS_SAFE}
        LIST_SEPARATOR |
        UPDATE_DISCONNECTED 1 # prevents from it always being built in search for tag change*
    )

    #define exec
    ExternalProject_Get_Property(peldd BINARY_DIR)
    set(PELDD_EXEC "${BINARY_DIR}/peldd")
endif()

################
# missing libs #
################

function(DeployPEDependencies target component)

    #add as dependency to ensure peldd is built beforehand
    add_dependencies(${target} peldd)

    # define output directory for main script
    set(PEDeps_${component}_DIR ${CMAKE_BINARY_DIR}/PEDeps_${component} PARENT_SCOPE)

    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/win/CMakeDependencies.in.sh
        PEDeps_${component}.sh
        NEWLINE_STYLE LF
    )

    # run script once target is built
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND bash PEDeps_${component}.sh $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_BINARY_DIR}/PEDeps_${component} $<TARGET_FILE_DIR:${target}>
    )

    # install
    install(DIRECTORY ${CMAKE_BINARY_DIR}/PEDeps_${component}/
        TYPE BIN
        COMPONENT ${component}
    )

endfunction()

##
##
##

endif()

SET(CMakeDependencies_INCLUDED ON)
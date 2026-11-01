# Add commands that copy the Qt runtime to the target's output directory after
# build and install the Qt runtime to the specified directory
macro(DeployQt target)
    # FOR WINDOWS
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        #
        # name and location differ between MSYS2 environments : look next to Qt's qmake first
        get_target_property(_qmake_location Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qmake_location}" DIRECTORY)
        find_program(DEPLOYQT_EXE
            NAMES windeployqt-qt6 windeployqt6 windeployqt
            HINTS "${_qt_bin_dir}"
            REQUIRED
        )

        #
        SET(QT_DEPS_OUTPUT_DIRNAME "QtRuntime")

        # Run deployqt immediately after build to determine Qt dependencies
        # SHOULD NOT BE "POST_BUILD" because pe-util might be used later on, and you want it to ignore QT libraries
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${DEPLOYQT_EXE}
                    --verbose 0
                    --no-opengl-sw
                    --no-quick-import
                    --no-system-d3d-compiler
                    --dir ${QT_DEPS_OUTPUT_DIRNAME}
                    --translations fr,en
                    $<TARGET_FILE:${target}>
            COMMENT "Create dummy folder with matching Qt runtime components"
        )

        # SHOULD NOT BE "POST_BUILD" because pe-util might be used later on, and you want it to ignore QT libraries
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${CMAKE_CURRENT_BINARY_DIR}/${QT_DEPS_OUTPUT_DIRNAME} $<TARGET_FILE_DIR:${target}>
        )

        #
        install(
            FILES 
                $<TARGET_FILE_DIR:${target}>/translations/qt_fr.qm
                $<TARGET_FILE_DIR:${target}>/translations/qt_en.qm
            DESTINATION ${CMAKE_INSTALL_BINDIR}/translations
            COMPONENT "App"
        )

        # install rule
        install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${QT_DEPS_OUTPUT_DIRNAME}/
            TYPE BIN
            COMPONENT "QtRuntime" # component name must NOT CHANGE since it is referred to elsewhere !
        )

    # FOR MACOS
    elseif(APPLE)
        #
        find_program(DEPLOYQT_EXE "macdeployqt" REQUIRED)

        ##
        ## Code signing is now required on OSX 13+, make sure to provide APPLE_CODESIGN_ID
        ##

        # Run deployqt immediately after build to determine Qt dependencies
        add_custom_command(TARGET ${target}
            COMMAND ${DEPLOYQT_EXE}
                $<TARGET_BUNDLE_DIR:${target}>
                -codesign=${APPLE_CODESIGN_ID}
                -always-overwrite
                # -verbose=3
            COMMENT "Create dummy folder with matching Qt runtime components"
        )
    endif()

endmacro()
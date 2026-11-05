# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Generates the icons from their drawings (resources/icons/svg) into resources/icons/build, out of version control,
# before what uses them : see resources/icons/generate.ts, whose outputs are listed here. Needs Bun (see cmake/webview.cmake)

SET(ICONS_DIR ${PROJECT_SOURCE_DIR}/resources/icons)

SET(ICONS_OUTPUTS
    app.ico
    package.ico
    app.icns
    package.icns
    app_reverse.png
    app_reverse_active.png
)
foreach(base dark light active)
    foreach(dot "" _ok _issue)
        list(APPEND ICONS_OUTPUTS tray_${base}${dot}.ico)
    endforeach()
endforeach()
list(TRANSFORM ICONS_OUTPUTS PREPEND ${ICONS_BUILD_DIR}/)

file(GLOB ICONS_DRAWINGS CONFIGURE_DEPENDS ${ICONS_DIR}/svg/*.svg)

add_custom_command(
    OUTPUT ${ICONS_OUTPUTS}
    COMMAND ${BUN_EXECUTABLE} install --frozen-lockfile
    COMMAND ${BUN_EXECUTABLE} generate.ts
    WORKING_DIRECTORY ${ICONS_DIR}
    DEPENDS ${ICONS_DRAWINGS} ${ICONS_DIR}/generate.ts ${ICONS_DIR}/package.json ${ICONS_DIR}/bun.lock
    COMMENT "Generating the icons..."
    VERBATIM
)

# built before the sources using them, which the resource compiler does not tell
target_sources(appExec PRIVATE ${ICONS_OUTPUTS})
if(WIN32)
    set_source_files_properties(resources/resources.rc PROPERTIES OBJECT_DEPENDS "${ICONS_OUTPUTS}")
endif()

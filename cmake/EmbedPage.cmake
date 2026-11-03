# SPDX-FileCopyrightText: 2018-2026 Guillaume Vara <guillaume.vara@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Embeds the built page into a C++ header, in CMake script mode : see cmake/webview.cmake
#   PAGE_HTML_FILE : the page, as built by Vite
#   TEMPLATE : templates/page.h.in
#   OUTPUT : the header

cmake_minimum_required(VERSION 3.25)

file(READ ${PAGE_HTML_FILE} PAGE_HTML)

# delimiter of the raw string literal holding the page
if(PAGE_HTML MATCHES "\\)soundbuddy_page\"")
    message(FATAL_ERROR "The page contains the delimiter of its C++ string : change it in templates/page.h.in")
endif()

configure_file(${TEMPLATE} ${OUTPUT} @ONLY)

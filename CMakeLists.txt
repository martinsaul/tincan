# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

cmake_minimum_required(VERSION 3.25)

project(tincan VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TINCAN_WERROR "Treat compiler warnings as errors on Tincan targets" OFF)

function(tincan_apply_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        $<$<BOOL:${TINCAN_WERROR}>:-Werror>
    )
endfunction()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Sql Network DBus Widgets Svg Test LinguistTools)
find_package(Qt6Keychain REQUIRED)
include(Pjproject)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests/unit)
add_subdirectory(tests/ui)
add_subdirectory(tests/e2e)

# Desktop integration files (see resources/linux).
set(TINCAN_APP_ID io.github.martinsaul.Tincan)
set(TINCAN_DESKTOP_FILE "${PROJECT_SOURCE_DIR}/resources/linux/${TINCAN_APP_ID}.desktop")
set(TINCAN_METAINFO_FILE "${PROJECT_SOURCE_DIR}/resources/linux/${TINCAN_APP_ID}.metainfo.xml")

install(FILES "${TINCAN_DESKTOP_FILE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${TINCAN_METAINFO_FILE}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
install(FILES "${PROJECT_SOURCE_DIR}/resources/icons/tincan.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    RENAME "${TINCAN_APP_ID}.svg")

find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
if(DESKTOP_FILE_VALIDATE)
    add_test(NAME validate_desktop_file
        COMMAND "${DESKTOP_FILE_VALIDATE}" "${TINCAN_DESKTOP_FILE}")
endif()
find_program(APPSTREAMCLI appstreamcli)
if(APPSTREAMCLI)
    add_test(NAME validate_metainfo
        COMMAND "${APPSTREAMCLI}" validate --no-net "${TINCAN_METAINFO_FILE}")
endif()

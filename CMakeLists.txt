# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

cmake_minimum_required(VERSION 3.25)

project(tincan VERSION 0.1.0 LANGUAGES CXX)

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

find_package(Qt6 REQUIRED COMPONENTS Core Sql Network Widgets Svg Test)
find_package(Qt6Keychain REQUIRED)
include(Pjproject)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests/unit)

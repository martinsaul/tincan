# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/core/version_config.h"
    @ONLY
)

add_library(tincan_core STATIC
    account_config.cpp
    sip_headers.cpp
    sip_uri.cpp
    version.cpp
)

target_include_directories(tincan_core
    PUBLIC
        "${CMAKE_SOURCE_DIR}/src"
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(tincan_core PUBLIC
    Qt6::Core
)

tincan_apply_warnings(tincan_core)

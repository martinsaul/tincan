# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_app STATIC
    app_paths.cpp
)

target_link_libraries(tincan_app
    PUBLIC
        tincan_core
        Qt6::Core
        Qt6::Sql
        Qt6::Network
        qt6keychain
)

tincan_apply_warnings(tincan_app)

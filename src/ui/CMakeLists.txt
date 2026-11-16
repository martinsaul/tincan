# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_ui STATIC
    main_window.h
    main_window.cpp
)

set_target_properties(tincan_ui PROPERTIES
    AUTOMOC ON
)

target_link_libraries(tincan_ui
    PUBLIC
        tincan_app
        Qt6::Widgets
        Qt6::Svg
)

tincan_apply_warnings(tincan_ui)

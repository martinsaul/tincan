# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_subdirectory(core)
add_subdirectory(sip)
add_subdirectory(app)
add_subdirectory(ui)

add_executable(tincan
    main.cpp
)

target_link_libraries(tincan PRIVATE
    tincan_ui
    tincan_app
    tincan_sip
    tincan_core
    Qt6::Widgets
)

tincan_apply_warnings(tincan)

# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_sip STATIC
    pj_version.cpp
)

target_link_libraries(tincan_sip
    PUBLIC
        tincan_core
    PRIVATE
        Tincan::pjproject
)

tincan_apply_warnings(tincan_sip)

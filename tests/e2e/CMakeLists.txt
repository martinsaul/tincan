# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_executable(tst_e2e_calls
    pjsua_peer.cpp
    recording_events.h
    recording_events.cpp
    tst_e2e_calls.cpp
)

set_target_properties(tst_e2e_calls PROPERTIES
    AUTOMOC ON
)

target_compile_definitions(tst_e2e_calls PRIVATE
    TINCAN_PJSUA_BINARY="${TINCAN_PJ_PREFIX}/bin/pjsua"
)

target_link_libraries(tst_e2e_calls PRIVATE
    Qt6::Network
    Qt6::Test
    tincan_sip
    tincan_core
)

tincan_apply_warnings(tst_e2e_calls)

add_test(NAME tst_e2e_calls COMMAND tst_e2e_calls)
set_tests_properties(tst_e2e_calls PROPERTIES LABELS e2e)

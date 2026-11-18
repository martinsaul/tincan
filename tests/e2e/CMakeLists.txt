# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

function(tincan_add_e2e_test test_name source_file)
    add_executable(${test_name}
        pjsua_peer.cpp
        recording_events.h
        recording_events.cpp
        ${source_file}
    )
    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
    )
    target_compile_definitions(${test_name} PRIVATE
        TINCAN_PJSUA_BINARY="${TINCAN_PJ_PREFIX}/bin/pjsua"
    )
    target_link_libraries(${test_name} PRIVATE
        Qt6::Network
        Qt6::Test
        tincan_sip
        tincan_core
    )
    tincan_apply_warnings(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES LABELS e2e)
endfunction()

tincan_add_e2e_test(tst_e2e_calls tst_e2e_calls.cpp)
tincan_add_e2e_test(tst_e2e_features tst_e2e_features.cpp)
tincan_add_e2e_test(tst_e2e_p4 tst_e2e_p4.cpp)
tincan_add_e2e_test(tst_e2e_mute tst_e2e_mute.cpp)
tincan_add_e2e_test(tst_e2e_reuse tst_e2e_reuse.cpp)

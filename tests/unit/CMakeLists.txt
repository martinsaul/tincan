# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

function(tincan_add_unit_test test_name source_file)
    add_executable(${test_name}
        ${source_file}
    )
    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
    )
    target_link_libraries(${test_name} PRIVATE
        Qt6::Test
        ${ARGN}
    )
    tincan_apply_warnings(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

tincan_add_unit_test(tst_version tst_version.cpp tincan_core)
tincan_add_unit_test(tst_apppaths tst_apppaths.cpp tincan_app)
tincan_add_unit_test(tst_pjversion tst_pjversion.cpp tincan_sip)

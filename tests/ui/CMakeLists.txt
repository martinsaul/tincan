# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

foreach(ui_test IN ITEMS tst_ui_smoke tst_ui_p4)
    add_executable(${ui_test}
        ${ui_test}.cpp
        "${CMAKE_SOURCE_DIR}/tests/fakes/fake_sip_engine.cpp"
    )

    set_target_properties(${ui_test} PROPERTIES
        AUTOMOC ON
    )

    target_include_directories(${ui_test} PRIVATE
        "${CMAKE_SOURCE_DIR}/tests/fakes"
    )

    target_link_libraries(${ui_test} PRIVATE
        Qt6::Test
        Qt6::Widgets
        tincan_ui
        tincan_app
        tincan_core
    )

    tincan_apply_warnings(${ui_test})

    add_test(NAME ${ui_test} COMMAND ${ui_test})
    set_tests_properties(${ui_test} PROPERTIES LABELS ui)
endforeach()

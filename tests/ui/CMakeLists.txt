# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_executable(tst_ui_smoke
    tst_ui_smoke.cpp
    "${CMAKE_SOURCE_DIR}/tests/fakes/fake_sip_engine.cpp"
)

set_target_properties(tst_ui_smoke PROPERTIES
    AUTOMOC ON
)

target_include_directories(tst_ui_smoke PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/fakes"
)

target_link_libraries(tst_ui_smoke PRIVATE
    Qt6::Test
    Qt6::Widgets
    tincan_ui
    tincan_app
    tincan_core
)

tincan_apply_warnings(tst_ui_smoke)

add_test(NAME tst_ui_smoke COMMAND tst_ui_smoke)
set_tests_properties(tst_ui_smoke PROPERTIES LABELS ui)

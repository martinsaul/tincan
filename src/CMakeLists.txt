# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_subdirectory(core)
add_subdirectory(sip)
add_subdirectory(app)
add_subdirectory(ui)

add_executable(tincan
    composition.cpp
    composition_screenshots.cpp
    main.cpp
    posix_signal_watcher.h
    posix_signal_watcher.cpp
)

set_target_properties(tincan PROPERTIES
    AUTOMOC ON
)

target_link_libraries(tincan PRIVATE
    tincan_ui
    tincan_app
    tincan_sip
    tincan_core
    Qt6::Widgets
)

tincan_apply_warnings(tincan)

# Translations: lupdate scans every Tincan library; the compiled .qm files are
# embedded under :/i18n and picked by the system locale at start-up.
qt_add_translations(TARGETS tincan
    SOURCE_TARGETS tincan tincan_ui tincan_app tincan_core
    TS_FILES "${PROJECT_SOURCE_DIR}/translations/tincan_pt_BR.ts"
    PLURALS_TS_FILE "${PROJECT_SOURCE_DIR}/translations/tincan_en.ts"
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete -locations none
)

install(TARGETS tincan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_ui STATIC
    "${CMAKE_SOURCE_DIR}/resources/tincan.qrc"
    about_dialog.h
    about_dialog.cpp
    account_dialog.h
    account_dialog.cpp
    accounts_page.h
    accounts_page.cpp
    audio_page.h
    audio_page.cpp
    call_card.h
    call_card.cpp
    call_panel.h
    call_panel.cpp
    calls_page.h
    calls_page.cpp
    chat_window.h
    chat_window.cpp
    contact_dialog.h
    contact_dialog.cpp
    contacts_view.h
    contacts_view.cpp
    dialpad_widget.h
    dialpad_widget.cpp
    dtmf_pad.h
    dtmf_pad.cpp
    general_page.h
    general_page.cpp
    header_bar.h
    header_bar.cpp
    history_view.h
    history_view.cpp
    icons.h
    icons.cpp
    incoming_call_popup.h
    incoming_call_popup.cpp
    main_window.h
    main_window.cpp
    messages_view.h
    messages_view.cpp
    secret_fallback_prompt.h
    secret_fallback_prompt.cpp
    settings_dialog.h
    settings_dialog.cpp
    transfer_dialog.h
    transfer_dialog.cpp
    tray_icon.h
    tray_icon.cpp
)

set_target_properties(tincan_ui PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tincan_ui
    PUBLIC
        tincan_app
        Qt6::Widgets
        Qt6::Svg
)

tincan_apply_warnings(tincan_ui)

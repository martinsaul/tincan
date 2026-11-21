# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_sip STATIC
    account_config_mapper.cpp
    buddy_registry.cpp
    call_controller.cpp
    call_info_mapper.cpp
    call_recorder.cpp
    call_registry.cpp
    call_slot.h
    call_slot.cpp
    conference_bridge.cpp
    event_marshaller.h
    event_marshaller.cpp
    keypad_tone_player.cpp
    pj_version.cpp
    pjsip_engine.cpp
    presence_mapper.cpp
    sip_account.cpp
    sip_buddy.cpp
    sip_call.cpp
    sip_endpoint.cpp
    sip_log_writer.cpp
    tone_player.cpp
)

set_target_properties(tincan_sip PROPERTIES
    AUTOMOC ON
)

target_link_libraries(tincan_sip
    PUBLIC
        tincan_core
    PRIVATE
        Tincan::pjproject
)

tincan_apply_warnings(tincan_sip)

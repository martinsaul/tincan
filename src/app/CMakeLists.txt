# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_app STATIC
    account_service.h
    account_service.cpp
    app_paths.cpp
    audio_service.h
    audio_service.cpp
    auto_answer_policy.cpp
    call_service.h
    call_service.cpp
    call_tone_controller.cpp
    forwarding_policy.cpp
    incoming_call_coordinator.h
    incoming_call_coordinator.cpp
    recording_namer.cpp
    secret_store.cpp
    settings_store.cpp
)

set_target_properties(tincan_app PROPERTIES
    AUTOMOC ON
)

target_link_libraries(tincan_app
    PUBLIC
        tincan_core
        Qt6::Core
        Qt6::Sql
        Qt6::Network
        qt6keychain
)

tincan_apply_warnings(tincan_app)

# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_app STATIC
    account_service.h
    account_service.cpp
    app_paths.cpp
    audio_service.h
    audio_service.cpp
    auto_answer_policy.cpp
    autostart.cpp
    call_service.h
    call_service.cpp
    call_tone_controller.cpp
    command_dispatcher.h
    command_dispatcher.cpp
    command_line.cpp
    config_secret_store.cpp
    contact_csv.cpp
    contact_repository.cpp
    contact_service.h
    contact_service.cpp
    database.cpp
    dbus_notifications.h
    dbus_notifications.cpp
    fallback_secret_store.cpp
    forwarding_policy.cpp
    history_repository.cpp
    history_service.h
    history_service.cpp
    incoming_call_coordinator.h
    incoming_call_coordinator.cpp
    message_repository.cpp
    message_service.h
    message_service.cpp
    migrations.cpp
    notification_backend.h
    notifier.h
    notifier.cpp
    private_directory.h
    private_directory.cpp
    recording_namer.cpp
    secret_store.cpp
    settings_store.cpp
    single_instance.h
    single_instance.cpp
    uri_parser.cpp
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
        Qt6::DBus
        qt6keychain
)

tincan_apply_warnings(tincan_app)

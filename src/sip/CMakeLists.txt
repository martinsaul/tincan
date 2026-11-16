# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

add_library(tincan_sip STATIC
    account_config_mapper.cpp
    call_info_mapper.cpp
    call_registry.cpp
    event_marshaller.h
    event_marshaller.cpp
    pj_version.cpp
    pjsip_engine.cpp
    sip_account.cpp
    sip_call.cpp
    sip_endpoint.cpp
    sip_log_writer.cpp
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

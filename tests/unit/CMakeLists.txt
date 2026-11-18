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
tincan_add_unit_test(tst_sipuri tst_sipuri.cpp tincan_core)
tincan_add_unit_test(tst_sipheaders tst_sipheaders.cpp tincan_core)
tincan_add_unit_test(tst_accountconfig tst_accountconfig.cpp tincan_core)
tincan_add_unit_test(tst_accountconfigmapper tst_accountconfigmapper.cpp tincan_sip Tincan::pjproject)
tincan_add_unit_test(tst_presencemapper tst_presencemapper.cpp tincan_sip Tincan::pjproject)
tincan_add_unit_test(tst_apppaths tst_apppaths.cpp tincan_app)
tincan_add_unit_test(tst_autoanswerpolicy tst_autoanswerpolicy.cpp tincan_app)
tincan_add_unit_test(tst_forwardingpolicy tst_forwardingpolicy.cpp tincan_app)
tincan_add_unit_test(tst_pjversion tst_pjversion.cpp tincan_sip)
tincan_add_unit_test(tst_settingsstore tst_settingsstore.cpp tincan_app)
tincan_add_unit_test(tst_database_migrations tst_database_migrations.cpp tincan_app)
tincan_add_unit_test(tst_repositories tst_repositories.cpp tincan_app)
tincan_add_unit_test(tst_contactcsv tst_contactcsv.cpp tincan_app)
tincan_add_unit_test(tst_uriparser tst_uriparser.cpp tincan_app)
tincan_add_unit_test(tst_commandline tst_commandline.cpp tincan_app)
tincan_add_unit_test(tst_singleinstance tst_singleinstance.cpp tincan_app)
tincan_add_unit_test(tst_privatedirectory tst_privatedirectory.cpp tincan_app)
tincan_add_unit_test(tst_autostart tst_autostart.cpp tincan_app)
tincan_add_unit_test(tst_configsecretstore tst_configsecretstore.cpp tincan_app)
tincan_add_unit_test(tst_recordingfile tst_recordingfile.cpp tincan_app)

foreach(svc_test IN ITEMS
        tst_accountservice
        tst_callservice
        tst_historyservice
        tst_contactservice
        tst_messageservice
        tst_notifier
        tst_commanddispatcher)
    add_executable(${svc_test}
        ${svc_test}.cpp
        "${CMAKE_SOURCE_DIR}/tests/fakes/fake_sip_engine.cpp"
    )
    set_target_properties(${svc_test} PROPERTIES
        AUTOMOC ON
    )
    target_include_directories(${svc_test} PRIVATE
        "${CMAKE_SOURCE_DIR}/tests/fakes"
    )
    target_link_libraries(${svc_test} PRIVATE
        Qt6::Test
        tincan_app
    )
    tincan_apply_warnings(${svc_test})
    add_test(NAME ${svc_test} COMMAND ${svc_test})
endforeach()

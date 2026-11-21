# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

# Distribution-specific details of the .deb, derived from the build machine
# so that one CPack configuration serves Debian and Ubuntu:
#
#   TINCAN_DEB_VERSION_SUFFIX  appended to the Debian version. Empty on Debian;
#                              "~ubuntu<VERSION_ID>" on Ubuntu, which sorts
#                              below the plain version and keeps the file names
#                              of both packages distinct.
#   tincan_qt_plugin_packages(<out>)
#                              the packages that ship the Qt plugins Tincan
#                              loads at run time (dpkg-shlibdeps cannot see
#                              them because they are not linked).

cmake_host_system_information(RESULT _tincan_os_id QUERY DISTRIB_ID)
cmake_host_system_information(RESULT _tincan_os_version QUERY DISTRIB_VERSION_ID)
if(_tincan_os_id STREQUAL "ubuntu" AND _tincan_os_version)
    set(_tincan_default_suffix "~ubuntu${_tincan_os_version}")
else()
    set(_tincan_default_suffix "")
endif()
set(TINCAN_DEB_VERSION_SUFFIX "${_tincan_default_suffix}" CACHE STRING
    "Suffix appended to the .deb version (default: ~ubuntu<release> on Ubuntu)")

# Plugins Tincan needs at run time, each with the Qt library it belongs to:
# the SQLite driver, the SVG icon engine and the X11 platform plugin. They
# are looked up by file because not every distribution's Qt packages ship
# the CMake plugin targets. A plugin packaged together with its library
# (Ubuntu) is already covered by dpkg-shlibdeps and is not listed again.
set(_tincan_runtime_qt_plugins
    "sqldrivers/libqsqlite.so|Qt6::Sql"
    "iconengines/libqsvgicon.so|Qt6::Svg"
    "platforms/libqxcb.so|Qt6::Gui")
# Used when the owners cannot be looked up (no dpkg, or Qt not from packages).
set(_tincan_fallback_plugin_packages libqt6sql6-sqlite qt6-svg-plugins)

# Sets <out> to the package that owns <file>, or to "" if none does.
function(_tincan_dpkg_owner file out)
    set(${out} "" PARENT_SCOPE)
    find_program(TINCAN_DPKG_QUERY dpkg-query)
    if(NOT TINCAN_DPKG_QUERY OR NOT EXISTS "${file}")
        return()
    endif()
    file(REAL_PATH "${file}" _real)
    execute_process(COMMAND "${TINCAN_DPKG_QUERY}" -S "${_real}"
        OUTPUT_VARIABLE _owner RESULT_VARIABLE _rc ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    # "libqt6svg6:amd64: /usr/lib/..." -> "libqt6svg6"
    if(_rc EQUAL 0 AND _owner MATCHES "^([a-z0-9][a-z0-9.+-]*)(:[a-z0-9]+)?: ")
        set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endif()
endfunction()

# Sets <out> to the package of the plugin in <entry> ("<file>|<qt-target>"),
# to "" when its library's package already ships it, or to "NOTFOUND".
function(_tincan_plugin_package entry out)
    string(REPLACE "|" ";" _parts "${entry}")
    list(GET _parts 0 _file)
    list(GET _parts 1 _library_target)
    _tincan_dpkg_owner("${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/${_file}" _owner)
    set(_library_owner "")
    if(TARGET ${_library_target})
        get_target_property(_library ${_library_target} LOCATION)
        _tincan_dpkg_owner("${_library}" _library_owner)
    endif()
    if(NOT _owner)
        set(${out} NOTFOUND PARENT_SCOPE)
    elseif(_owner STREQUAL _library_owner)
        set(${out} "" PARENT_SCOPE)
    else()
        set(${out} "${_owner}" PARENT_SCOPE)
    endif()
endfunction()

function(tincan_qt_plugin_packages out)
    set(_packages "")
    foreach(_entry IN LISTS _tincan_runtime_qt_plugins)
        _tincan_plugin_package("${_entry}" _package)
        if(_package STREQUAL "NOTFOUND")
            message(STATUS "No package owns ${_entry}; using the default plugin packages")
            set(${out} "${_tincan_fallback_plugin_packages}" PARENT_SCOPE)
            return()
        endif()
        list(APPEND _packages ${_package})
    endforeach()
    list(REMOVE_DUPLICATES _packages)
    set(${out} "${_packages}" PARENT_SCOPE)
endfunction()

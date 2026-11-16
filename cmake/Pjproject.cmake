# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

set(TINCAN_PJ_PREFIX "${CMAKE_SOURCE_DIR}/third_party/pjproject/install"
    CACHE PATH "Installation prefix of the pinned pjproject build")

set(_tincan_pj_pc_dir "${TINCAN_PJ_PREFIX}/lib/pkgconfig")
if(NOT EXISTS "${_tincan_pj_pc_dir}/libpjproject.pc")
    message(FATAL_ERROR
        "Pinned pjproject 2.17 installation not found at '${TINCAN_PJ_PREFIX}' "
        "(missing '${_tincan_pj_pc_dir}/libpjproject.pc').\n"
        "Build it first by running:\n"
        "  tools/build-pjproject.sh")
endif()

if(DEFINED ENV{PKG_CONFIG_PATH} AND NOT "$ENV{PKG_CONFIG_PATH}" STREQUAL "")
    set(ENV{PKG_CONFIG_PATH} "${_tincan_pj_pc_dir}:$ENV{PKG_CONFIG_PATH}")
else()
    set(ENV{PKG_CONFIG_PATH} "${_tincan_pj_pc_dir}")
endif()

find_package(PkgConfig REQUIRED)

set(_tincan_saved_pkg_config_argn "${PKG_CONFIG_ARGN}")
list(APPEND PKG_CONFIG_ARGN "--static")
pkg_check_modules(PJPROJECT REQUIRED IMPORTED_TARGET libpjproject)
set(PKG_CONFIG_ARGN "${_tincan_saved_pkg_config_argn}")
unset(_tincan_saved_pkg_config_argn)

set_target_properties(PkgConfig::PJPROJECT PROPERTIES SYSTEM TRUE)
if(PJPROJECT_INCLUDE_DIRS)
    target_include_directories(PkgConfig::PJPROJECT SYSTEM INTERFACE ${PJPROJECT_INCLUDE_DIRS})
endif()

get_target_property(_tincan_pj_libs PkgConfig::PJPROJECT INTERFACE_LINK_LIBRARIES)
if(_tincan_pj_libs)
    set_target_properties(PkgConfig::PJPROJECT PROPERTIES
        INTERFACE_LINK_LIBRARIES "$<LINK_GROUP:RESCAN,${_tincan_pj_libs}>"
    )
endif()
unset(_tincan_pj_libs)

if(NOT TARGET Tincan::pjproject)
    add_library(Tincan::pjproject ALIAS PkgConfig::PJPROJECT)
endif()

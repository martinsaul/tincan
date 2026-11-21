# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

# Documentation install rules and the CPack .deb configuration.
#   cmake --build build-rel && (cd build-rel && cpack -G DEB)

# --- Man page (gzip-compressed when gzip is available, as Debian expects) ---
set(_tincan_man_src "${PROJECT_SOURCE_DIR}/man/tincan.1")
find_program(TINCAN_GZIP gzip)
if(TINCAN_GZIP)
    set(_tincan_man_out "${PROJECT_BINARY_DIR}/man/tincan.1.gz")
    add_custom_command(OUTPUT "${_tincan_man_out}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/man"
        COMMAND "${TINCAN_GZIP}" -9 -n -c "${_tincan_man_src}" > "${_tincan_man_out}"
        DEPENDS "${_tincan_man_src}"
        COMMENT "Compressing man page"
        VERBATIM)
    add_custom_target(tincan_man ALL DEPENDS "${_tincan_man_out}")
else()
    set(_tincan_man_out "${_tincan_man_src}")
endif()
install(FILES "${_tincan_man_out}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

# --- Licence and notices: share/doc/tincan ---
set(TINCAN_DOC_DIR "${CMAKE_INSTALL_DATADIR}/doc/tincan")
install(FILES
    "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${PROJECT_SOURCE_DIR}/README.md"
    "${PROJECT_SOURCE_DIR}/CHANGELOG.md"
    DESTINATION "${TINCAN_DOC_DIR}")
# Machine-readable (DEP-5) copyright file, as Debian policy requires.
install(FILES "${PROJECT_SOURCE_DIR}/packaging/debian/copyright"
    DESTINATION "${TINCAN_DOC_DIR}")

# --- CPack: Debian package ---
include(DebianDistro)
set(CPACK_GENERATOR "DEB" CACHE STRING "CPack generators")
set(CPACK_PACKAGE_NAME "tincan")
set(CPACK_PACKAGE_VENDOR "Tincan contributors")
set(CPACK_PACKAGE_CONTACT "Tincan contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES ON)
# Packaged directories must be 0755 whatever the builder's umask or ACLs are.
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
set(CPACK_PRE_BUILD_SCRIPTS "${PROJECT_SOURCE_DIR}/cmake/CPackNormalizePermissions.cmake")

set(CPACK_DEBIAN_PACKAGE_NAME "tincan")
# Debian version <version><suffix>; the suffix is empty on Debian.
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}${TINCAN_DEB_VERSION_SUFFIX}")
if(TINCAN_DEB_VERSION_SUFFIX)
    # GitHub stores "~" in release asset names as ".", so the file name uses
    # "_<tag>" instead: tincan_0.1.2_ubuntu24.04_amd64.deb.
    string(REGEX REPLACE "^~" "" _tincan_deb_tag "${TINCAN_DEB_VERSION_SUFFIX}")
    execute_process(COMMAND dpkg --print-architecture
        OUTPUT_VARIABLE _tincan_deb_arch OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT _tincan_deb_arch)
        set(_tincan_deb_arch amd64)
    endif()
    set(CPACK_DEBIAN_FILE_NAME
        "tincan_${PROJECT_VERSION}_${_tincan_deb_tag}_${_tincan_deb_arch}.deb")
else()
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tincan contributors")
set(CPACK_DEBIAN_PACKAGE_SECTION "comm")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE_URL}")
# CPack prepends CPACK_PACKAGE_DESCRIPTION_SUMMARY as the synopsis line.
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
    "Tincan is a compact SIP softphone for Linux desktops built with Qt 6 and\n\
PJSIP: several accounts, hold, transfer, conference, recording, contacts\n\
with presence, call history, SIP MESSAGE chat and a system tray icon.")
# Linked libraries come from dpkg-shlibdeps. Qt loads the SQLite driver, the
# SVG icon engine and the platform plugin at run time; the packages that ship
# them differ between distributions, so ask dpkg which ones they are.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
tincan_qt_plugin_packages(_tincan_plugin_packages)
list(JOIN _tincan_plugin_packages ", " CPACK_DEBIAN_PACKAGE_DEPENDS)
message(STATUS "Tincan .deb: version ${CPACK_DEBIAN_PACKAGE_VERSION}, "
    "plugin packages: ${CPACK_DEBIAN_PACKAGE_DEPENDS}")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "gnome-keyring | kwalletmanager | keepassxc")

include(CPack)

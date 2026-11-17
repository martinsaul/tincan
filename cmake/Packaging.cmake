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
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
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
# SVG icon engine and the platform plugins at run time, so list them here.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6sql6-sqlite, qt6-svg-plugins, qt6-qpa-plugins")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "gnome-keyring | kwalletmanager | keepassxc")

include(CPack)

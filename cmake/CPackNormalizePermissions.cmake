# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

# CPack pre-build script: give every staged directory mode 0755 and clear
# setgid bits, so the package does not inherit default ACLs or an unusual
# umask from the build machine.
if(NOT CPACK_TEMPORARY_INSTALL_DIRECTORY)
    return()
endif()
file(GLOB_RECURSE _tincan_staged LIST_DIRECTORIES true
    "${CPACK_TEMPORARY_INSTALL_DIRECTORY}/*")
foreach(_path IN LISTS _tincan_staged)
    if(IS_DIRECTORY "${_path}" AND NOT IS_SYMLINK "${_path}")
        file(CHMOD "${_path}" PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)
    endif()
endforeach()

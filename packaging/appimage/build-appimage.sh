#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors
#
# Builds Tincan-<version>-x86_64.AppImage with linuxdeploy and
# linuxdeploy-plugin-qt.
#
#   packaging/appimage/build-appimage.sh
#
# Steps: build a Release tree (tests off) in build-appimage/, install it into
# a temporary AppDir with prefix /usr, download the pinned linuxdeploy tools
# into build/tools/ (SHA-256 verified), then bundle the libraries, Qt plugins
# and libqt6keychain and write the AppImage to the output directory.
#
# Environment (all optional):
#   TINCAN_APPIMAGE_BUILD_DIR    Release build tree   (default: build-appimage)
#   TINCAN_APPIMAGE_OUTPUT_DIR   Output directory     (default: the build tree)
#   LINUXDEPLOY_SHA256           Override the pinned linuxdeploy checksum
#   LINUXDEPLOY_PLUGIN_QT_SHA256 Override the pinned plugin-qt checksum
#   LINUXDEPLOY / LINUXDEPLOY_PLUGIN_QT
#                                Use already-downloaded tool files instead
#                                (still checksum-verified; the plugin file
#                                must be named linuxdeploy-plugin-qt-x86_64.AppImage)
#   QMAKE                        qmake of the Qt 6 to bundle (default: qmake6)
#
# Checksum provenance:
#   - linuxdeploy 1-alpha-20251107-1: SHA-256 published by GitHub for the
#     release asset (the "digest" field of the releases API).
#   - linuxdeploy-plugin-qt 1-alpha-20250213-1: upstream publishes no
#     checksum for this release; the value below was computed from the
#     downloaded asset when it was pinned. Set LINUXDEPLOY_PLUGIN_QT_SHA256
#     to a value you verified yourself to override it.
#
# The AppImage runs on systems with a glibc at least as new as the build
# machine's. Building inside a container sets that baseline (see
# .github/workflows/release.yml).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TOOLS_DIR="${REPO_ROOT}/build/tools"
BUILD_DIR="${TINCAN_APPIMAGE_BUILD_DIR:-${REPO_ROOT}/build-appimage}"
OUTPUT_DIR="${TINCAN_APPIMAGE_OUTPUT_DIR:-${BUILD_DIR}}"
APP_ID="io.github.martinsaul.Tincan"
WORK_DIR="" # temporary AppDir and tool scratch space, removed on exit

LINUXDEPLOY_VERSION="1-alpha-20251107-1"
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_PINNED_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"

PLUGIN_QT_VERSION="1-alpha-20250213-1"
PLUGIN_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${PLUGIN_QT_VERSION}/linuxdeploy-plugin-qt-x86_64.AppImage"
PLUGIN_QT_PINNED_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"

die() {
    echo "ERROR: $*" >&2
    exit 1
}

verify_sha256() {
    local file="$1" expected="$2" actual
    [[ "${expected}" =~ ^[0-9a-f]{64}$ ]] || die "invalid SHA-256 for ${file}: '${expected}'"
    actual="$(sha256sum "${file}" | awk '{print $1}')"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "ERROR: SHA-256 mismatch for ${file}" >&2
        echo "  expected: ${expected}" >&2
        echo "  actual:   ${actual}" >&2
        exit 1
    fi
}

# fetch_tool <override-path> <url> <file-name> <sha256>; prints the tool path.
fetch_tool() {
    local override="$1" url="$2" name="$3" sha="$4"
    local target="${TOOLS_DIR}/${name}"
    if [[ -n "${override}" ]]; then
        [[ -f "${override}" ]] || die "tool not found: ${override}"
        target="${override}"
    elif [[ ! -f "${target}" ]]; then
        mkdir -p "${TOOLS_DIR}"
        echo "Downloading ${url}" >&2
        curl -fsSL --retry 3 -o "${target}.tmp" "${url}" \
            || die "download failed: ${url} (set LINUXDEPLOY / LINUXDEPLOY_PLUGIN_QT to local copies)"
        mv "${target}.tmp" "${target}"
    fi
    verify_sha256 "${target}" "${sha}"
    chmod +x "${target}"
    printf '%s\n' "${target}"
}

build_release() {
    if [[ ! -f "${REPO_ROOT}/third_party/pjproject/install/lib/pkgconfig/libpjproject.pc" ]]; then
        "${REPO_ROOT}/tools/build-pjproject.sh"
    fi
    cmake -G Ninja -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DTINCAN_BUILD_TESTS=OFF
    cmake --build "${BUILD_DIR}"
}

project_version() {
    local version
    version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "${BUILD_DIR}/CMakeCache.txt")"
    [[ -n "${version}" ]] || die "could not read the project version from ${BUILD_DIR}"
    printf '%s\n' "${version}"
}

# The QtKeychain library the tincan binary actually resolves to.
keychain_library() {
    local lib
    lib="$(ldd "${BUILD_DIR}/src/tincan" | awk '/libqt6keychain\.so/ {print $3; exit}')"
    [[ -n "${lib}" && -f "${lib}" ]] || die "libqt6keychain not found (install qtkeychain-qt6-dev)"
    printf '%s\n' "${lib}"
}

# Extra platform plugins: offscreen (so --help/--version work without a
# display) and Wayland when qt6-wayland is installed.
extra_platform_plugins() {
    local plugins_dir list="libqoffscreen.so"
    plugins_dir="$("${QMAKE}" -query QT_INSTALL_PLUGINS)"
    if [[ -f "${plugins_dir}/platforms/libqwayland-generic.so" ]]; then
        list="${list};libqwayland-egl.so;libqwayland-generic.so"
    fi
    printf '%s\n' "${list}"
}

# NSS, the crypto backend of Debian's and Ubuntu's libsrtp2, loads its
# PKCS #11 soft token with dlopen() from the directory of libnss3, so
# linuxdeploy cannot find it. Without it SRTP fails to initialise on hosts
# that have no NSS of their own. Prints one library path per line.
nss_modules() {
    local nss dir module
    nss="$(ldd "${BUILD_DIR}/src/tincan" | awk '/libnss3\.so/ {print $3; exit}')"
    [[ -n "${nss}" ]] || return 0
    dir="$(dirname "$(readlink -f "${nss}")")"
    for module in libsoftokn3.so libfreeblpriv3.so libfreebl3.so; do
        [[ -f "${dir}/${module}" ]] || die "NSS module ${module} not found in ${dir}"
        printf '%s\n' "${dir}/${module}"
    done
}

run_linuxdeploy() {
    local appdir="$1" linuxdeploy="$2" plugin_dir="$3" version="$4" output="$5"
    local platforms keychain module
    local -a libraries
    platforms="$(extra_platform_plugins)"
    keychain="$(keychain_library)" || exit 1
    libraries=(--library "${keychain}")
    while IFS= read -r module; do
        libraries+=(--library "${module}")
    done < <(nss_modules)
    # linuxdeploy finds linuxdeploy-plugin-qt-x86_64.AppImage on PATH.
    (
        cd "${OUTPUT_DIR}"
        export PATH="${plugin_dir}:${PATH}"
        export APPIMAGE_EXTRACT_AND_RUN=1 # no FUSE needed (containers, CI)
        export TMPDIR="${WORK_DIR}/tmp"   # the tools extract themselves here
        export QMAKE LINUXDEPLOY_OUTPUT_VERSION="${version}" OUTPUT="${output}"
        export EXTRA_PLATFORM_PLUGINS="${platforms}"
        # QtKeychain is listed explicitly so the AppImage never relies on a
        # host copy of it (or on whichever keychain backend that copy uses);
        # so are the NSS modules (see nss_modules).
        "${linuxdeploy}" \
            --appdir "${appdir}" \
            --executable "${appdir}/usr/bin/tincan" \
            --desktop-file "${appdir}/usr/share/applications/${APP_ID}.desktop" \
            --icon-file "${appdir}/usr/share/icons/hicolor/scalable/apps/${APP_ID}.svg" \
            "${libraries[@]}" \
            --plugin qt \
            --output appimage
    )
}

check_appdir() {
    local appdir="$1"
    compgen -G "${appdir}/usr/lib/libqt6keychain.so*" >/dev/null \
        || die "libqt6keychain was not bundled"
    [[ -f "${appdir}/usr/plugins/sqldrivers/libqsqlite.so" ]] \
        || die "Qt SQLite driver was not bundled"
    [[ -f "${appdir}/usr/plugins/platforms/libqxcb.so" ]] \
        || die "Qt xcb platform plugin was not bundled"
    if compgen -G "${appdir}/usr/lib/libnss3.so*" >/dev/null; then
        [[ -f "${appdir}/usr/lib/libsoftokn3.so" ]] || die "NSS soft token was not bundled"
    fi
}

main() {
    case "${1:-}" in
        -h | --help)
            sed -n '4,37p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        "") ;;
        *) die "unknown argument: $1" ;;
    esac
    command -v curl >/dev/null || die "curl is required"
    QMAKE="${QMAKE:-$(command -v qmake6 || true)}"
    [[ -x "${QMAKE}" ]] || die "qmake6 not found; set QMAKE to the Qt 6 qmake"

    local linuxdeploy
    linuxdeploy="$(fetch_tool "${LINUXDEPLOY:-}" "${LINUXDEPLOY_URL}" \
        linuxdeploy-x86_64.AppImage "${LINUXDEPLOY_SHA256:-${LINUXDEPLOY_PINNED_SHA256}}")"
    local plugin_qt
    plugin_qt="$(fetch_tool "${LINUXDEPLOY_PLUGIN_QT:-}" "${PLUGIN_QT_URL}" \
        linuxdeploy-plugin-qt-x86_64.AppImage \
        "${LINUXDEPLOY_PLUGIN_QT_SHA256:-${PLUGIN_QT_PINNED_SHA256}}")"

    build_release
    local version output appdir
    version="$(project_version)"
    output="Tincan-${version}-x86_64.AppImage"
    WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/tincan-appimage.XXXXXX")"
    trap 'rm -rf "${WORK_DIR}"' EXIT
    appdir="${WORK_DIR}/AppDir"
    mkdir -p "${appdir}" "${WORK_DIR}/tmp"

    DESTDIR="${appdir}" cmake --install "${BUILD_DIR}"
    mkdir -p "${OUTPUT_DIR}"
    rm -f "${OUTPUT_DIR}/${output}"
    run_linuxdeploy "${appdir}" "${linuxdeploy}" "$(dirname "${plugin_qt}")" "${version}" \
        "${output}"
    check_appdir "${appdir}"
    [[ -f "${OUTPUT_DIR}/${output}" ]] || die "linuxdeploy did not produce ${output}"
    echo "AppImage written to ${OUTPUT_DIR}/${output}"
}

main "$@"

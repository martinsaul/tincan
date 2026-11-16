#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

set -euo pipefail

PJ_VERSION="2.17"
PJ_URL="https://github.com/pjsip/pjproject/archive/refs/tags/${PJ_VERSION}.tar.gz"
PJ_SHA256="065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PJ_BASE_DIR="${REPO_ROOT}/third_party/pjproject"
PJ_SRC_DIR="${PJ_BASE_DIR}/src"
DEFAULT_PREFIX="${PJ_BASE_DIR}/install"

resolve_prefix() {
    local raw="${PJ_PREFIX:-${DEFAULT_PREFIX}}"
    mkdir -p "${raw}"
    (cd "${raw}" && pwd)
}

verify_sha256() {
    local tarball="$1"
    local actual
    actual="$(sha256sum "${tarball}" | awk '{print $1}')"
    if [[ "${actual}" != "${PJ_SHA256}" ]]; then
        echo "ERROR: SHA-256 mismatch for ${tarball}" >&2
        echo "  expected: ${PJ_SHA256}" >&2
        echo "  actual:   ${actual}" >&2
        exit 1
    fi
}

obtain_tarball() {
    if [[ -n "${PJ_TARBALL:-}" ]]; then
        if [[ ! -f "${PJ_TARBALL}" ]]; then
            echo "ERROR: PJ_TARBALL does not exist: ${PJ_TARBALL}" >&2
            exit 1
        fi
        verify_sha256 "${PJ_TARBALL}"
        printf '%s\n' "${PJ_TARBALL}"
        return
    fi

    local cached="${PJ_BASE_DIR}/pjproject-${PJ_VERSION}.tar.gz"
    mkdir -p "${PJ_BASE_DIR}"
    if [[ ! -f "${cached}" ]]; then
        echo "Downloading pjproject ${PJ_VERSION} from ${PJ_URL}..." >&2
        curl -fsSL -o "${cached}.tmp" "${PJ_URL}"
        mv "${cached}.tmp" "${cached}"
    fi
    verify_sha256 "${cached}"
    printf '%s\n' "${cached}"
}

prepare_source() {
    local tarball="$1"
    rm -rf "${PJ_SRC_DIR}"
    mkdir -p "${PJ_SRC_DIR}"
    tar -xzf "${tarball}" --strip-components=1 -C "${PJ_SRC_DIR}"

    cat > "${PJ_SRC_DIR}/pjlib/include/pj/config_site.h" <<'EOF'
#ifndef TINCAN_PJ_CONFIG_SITE_H
#define TINCAN_PJ_CONFIG_SITE_H

#define PJMEDIA_HAS_VIDEO 0
#define PJ_HAS_IPV6 1

#endif
EOF
}

configure_and_build() {
    local prefix="$1"
    pushd "${PJ_SRC_DIR}" >/dev/null
    # Note on dropped flags for pjproject 2.17:
    # - --with-external-opus: unrecognized by 2.17 configure (system libopus-dev
    #   is autodetected automatically; pjproject does not bundle Opus).
    # - --with-external-speex: requires both <speex/speex.h> (libspeex-dev) and
    #   <speex/speex_echo.h> (libspeexdsp-dev); when libspeex-dev is absent,
    #   configure aborts, whereas pjproject's bundled speex builds both cleanly.
    ./configure \
        --prefix="${prefix}" \
        --disable-video \
        --disable-sdl \
        --disable-ffmpeg \
        --disable-v4l2 \
        --disable-openh264 \
        --disable-libyuv \
        --disable-libwebrtc \
        --with-external-srtp \
        --enable-shared=no \
        CFLAGS="-O2 -fPIC" \
        CXXFLAGS="-O2 -fPIC"
    make dep
    make -j"$(nproc)"
    make install
    popd >/dev/null
}

install_pjsua_binary() {
    local prefix="$1"
    local candidates=("${PJ_SRC_DIR}"/pjsip-apps/bin/pjsua-*)
    if [[ ! -f "${candidates[0]}" ]]; then
        echo "ERROR: Built pjsua binary not found in ${PJ_SRC_DIR}/pjsip-apps/bin/" >&2
        exit 1
    fi
    mkdir -p "${prefix}/bin"
    install -m 0755 "${candidates[0]}" "${prefix}/bin/pjsua"
}

main() {
    local force=0
    for arg in "$@"; do
        case "${arg}" in
            --force) force=1 ;;
            -h|--help)
                echo "Usage: $0 [--force]"
                exit 0
                ;;
            *)
                echo "ERROR: Unknown argument: ${arg}" >&2
                exit 1
                ;;
        esac
    done

    local prefix
    prefix="$(resolve_prefix)"
    local pc_file="${prefix}/lib/pkgconfig/libpjproject.pc"

    if [[ "${force}" -eq 0 && -f "${pc_file}" ]]; then
        echo "pjproject ${PJ_VERSION} already installed at ${prefix} (use --force to rebuild)."
        exit 0
    fi

    local tarball
    tarball="$(obtain_tarball)"
    prepare_source "${tarball}"
    configure_and_build "${prefix}"
    install_pjsua_binary "${prefix}"
    echo "pjproject ${PJ_VERSION} installed to ${prefix}"
}

main "$@"

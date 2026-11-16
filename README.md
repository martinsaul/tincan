# Tincan

Tincan is a lightweight, fast SIP softphone for Linux desktops, written in C++20 with Qt 6 Widgets and PJSIP 2.17 (`pjsua2`).

## Status

**Pre-alpha** — Phase P0 skeleton (layered build, pinned PJSIP 2.17 toolchain, test harness, and CI).

## Building from source

Install build dependencies (Debian 13 / Trixie):

```sh
sudo apt-get install \
  build-essential cmake ninja-build pkg-config curl ca-certificates \
  qt6-base-dev qt6-tools-dev qt6-svg-dev qt6-l10n-tools \
  qtkeychain-qt6-dev libqt6sql6-sqlite \
  libsrtp2-dev libopus-dev libspeexdsp-dev \
  libpulse-dev libasound2-dev libssl-dev uuid-dev xvfb
```

Build the pinned PJSIP 2.17 static libraries, then configure, build, and test Tincan:

```sh
tools/build-pjproject.sh
cmake -G Ninja -B build
cmake --build build
ctest --test-dir build
```

For headless environments (such as CI), run tests with `QT_QPA_PLATFORM=offscreen`:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

## Inspiration and credit

Tincan is inspired by [MicroSIP](https://www.microsip.org), a lightweight GPL Windows softphone by its own authors. Tincan is an independent clean-room implementation that contains no MicroSIP code or assets and is not affiliated with or endorsed by the MicroSIP project.

Tincan builds upon:
- [PJSIP](https://www.pjsip.org) (`pjproject` / `pjsua2`) for SIP signaling and media processing.
- [Qt 6](https://www.qt.io) and QtKeychain for the desktop interface, persistence, and credential storage.

## License

Tincan is free software licensed under the GNU Affero General Public License v3.0 or later (`AGPL-3.0-or-later`). See [LICENSE](LICENSE) for the full license text.

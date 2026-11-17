# Third-party notices

Tincan itself is licensed under the GNU Affero General Public License,
version 3 or later (`AGPL-3.0-or-later`, see [LICENSE](LICENSE)). It is built
on the components below. All of them are compatible with the AGPL; the
combined work is distributed under AGPL-3.0-or-later.

"Linked" says how the component reaches a Tincan binary:

- **static**: compiled into the `tincan` executable in every package;
- **dynamic**: a shared library. The `.deb` takes it from the system (see the
  package's `Depends`). The AppImage bundles a copy, except for the base
  system libraries that linuxdeploy never bundles (glibc, libGL, X11/Wayland
  client libraries and similar).

Versions are the ones used by the release builds (Debian 13 "trixie"
packages, plus the pinned pjproject tarball). A `.deb` installed on another
system uses whatever compatible version that system provides.

## Compiled into Tincan

| Component | Version | License | Linked | Upstream |
|---|---|---|---|---|
| pjproject (PJSIP, PJMEDIA, PJNATH, PJLIB, pjsua2) | 2.17 | GPL-2.0-or-later | static | https://github.com/pjsip/pjproject |

pjproject is built from the release tarball
`https://github.com/pjsip/pjproject/archive/refs/tags/2.17.tar.gz`
(SHA-256 `065fe06c06788d97c35f563796d59f00ce52fe9558a52d7b490a042a966facce`)
by `tools/build-pjproject.sh` with video, SDL, FFmpeg, V4L2, OpenH264,
libyuv and libwebrtc disabled and with the system libsrtp2. The G.711,
G.722 and L16 codecs, the conference bridge, the echo canceller front end
and the rest of the stack are pjproject's own code under its GPL-2.0-or-later
licence.

pjproject ships the following libraries in its `third_party/` directory. The
build compiles these into Tincan:

| Bundled library | Version (per pjproject) | License | Licence file in the pjproject tree |
|---|---|---|---|
| Speex (Speex codec, AEC, preprocessor, resampler) | SVN r12832 | BSD-3-Clause (Xiph.Org) | `third_party/speex/COPYING` |
| GSM 06.10 | 1.0.12 | TU Berlin licence (permissive; keep the notice, no warranty) | `third_party/gsm/COPYRIGHT` |
| iLBC (RFC 3951 reference code) | from RFC 3951 | Copyright (C) The Internet Society (2004), distributed under the IETF RFC copyright terms | none; notice in each source file |
| G.722.1 (ITU-T G.722.1 reference code, adapted by Teluu) | as shipped in 2.17 | GPL-2.0-or-later (per-file headers) | none; notice in each source file |
| libresample | 1.7 | LGPL-2.1-or-later | `third_party/resample/COPYING`, `README.resample` |

These bundled libraries are **not** compiled in:

| Bundled library | Why |
|---|---|
| libsrtp (bundled copy) | `--with-external-srtp`: the system libsrtp2 is used instead |
| WebRTC AEC / AEC3 | `--disable-libwebrtc` |
| libyuv | `--disable-libyuv` (no video) |
| milenage, mp3, BaseClasses, bdsound, threademulation | not enabled by the configure options above |

G.722.1 is an ITU-T standard; its use may be subject to patent licensing
terms of the standard's contributors. Check the ITU-T and pjproject
documentation if you redistribute builds in a jurisdiction where that matters.

## Shared libraries

| Component | Version | License | Linked | Upstream |
|---|---|---|---|---|
| Qt 6 (Core, Gui, Widgets, Network, DBus, Sql, Svg, and the platform, SQLite and SVG plugins) | 6.8.2 | LGPL-3.0-only (Qt is also available under GPL and commercial terms) | dynamic | https://www.qt.io |
| QtKeychain | 0.15.0 | BSD-2-Clause | dynamic | https://github.com/frankosterfeld/qtkeychain |
| libopus | 1.5.2 | BSD-3-Clause | dynamic | https://opus-codec.org |
| libsrtp2 | 2.7.0 | BSD-3-Clause | dynamic | https://github.com/cisco/libsrtp |
| OpenSSL (libssl, libcrypto) | 3.5 | Apache-2.0 | dynamic | https://www.openssl.org |
| SQLite (through the Qt SQLite driver) | 3.46.1 | Public domain | dynamic | https://www.sqlite.org |
| ALSA library (libasound) | 1.2.14 | LGPL-2.1-or-later | dynamic | https://www.alsa-project.org |
| libuuid (util-linux) | 2.41 | BSD-3-Clause | dynamic | https://github.com/util-linux/util-linux |

Qt is used unmodified through its public API and linked dynamically, so you
can replace the Qt libraries with your own build (LGPL-3.0 section 4). In the
AppImage the Qt libraries are ordinary files under `usr/lib` and
`usr/plugins`; extract the AppImage with `--appimage-extract` to replace them.

The AppImage also contains the shared libraries these components depend on
(for example the GCC C++ runtime, GCC Runtime Library Exception, and the
libraries pulled in by the Qt plugins). Their licences are the ones of the
corresponding Debian/Ubuntu packages.

## Product inspiration

Tincan is inspired by [MicroSIP](https://www.microsip.org). It contains no
MicroSIP code, artwork, sounds or text and is not affiliated with the
MicroSIP project.

## Full licence texts

- AGPL-3.0: [LICENSE](LICENSE), or https://www.gnu.org/licenses/agpl-3.0.html
- GPL-2.0: https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
- LGPL-2.1: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
- LGPL-3.0: https://www.gnu.org/licenses/lgpl-3.0.html
- Apache-2.0: https://www.apache.org/licenses/LICENSE-2.0
- BSD-2-Clause, BSD-3-Clause: https://opensource.org/license/bsd-2-clause,
  https://opensource.org/license/bsd-3-clause, and the copyright files of the
  respective projects.

On Debian-based systems the texts are also in `/usr/share/common-licenses`,
and `/usr/share/doc/tincan/copyright` lists the same information in
machine-readable form.

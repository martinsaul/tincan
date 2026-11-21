# Tincan design

Tincan is a small, fast SIP softphone for Linux desktops. Its goal is the
experience of MicroSIP on Windows: a compact window, a dialpad, quick
answer and hang-up, and very little in the way. It is free software under
the GNU AGPL v3 or later.

## Relationship to MicroSIP

MicroSIP (https://www.microsip.org) is a GPL-licensed Windows softphone by
its own authors. Tincan uses it **only as a product reference**: which
features a lightweight softphone should have and how small it can feel.

- Tincan contains **no MicroSIP source code**, assets, icons, ringtones,
  translations or UI strings.
- Contributors (human or AI) must not read, paste or port MicroSIP source
  while working on Tincan. Behaviour is designed from the SIP RFCs, the
  PJSIP documentation and our own judgement.
- The README credits MicroSIP as the inspiration and links to it. Tincan is
  not affiliated with or endorsed by the MicroSIP project.

## Technology

| Concern | Choice | Why |
|---|---|---|
| Language | C++20 | PJSIP and Qt are both native; no binding layer |
| UI | Qt 6 Widgets | Compact native-looking desktop UI, tray, D-Bus, i18n |
| SIP/media | PJSIP 2.17 via the `pjsua2` C++ API | Mature: codecs, AEC, ICE/STUN/TURN, TLS, SRTP, presence, IM |
| Audio backend | PJSIP ALSA (works through PipeWire/PulseAudio ALSA plugins) | No extra glue |
| Storage | SQLite through QtSql (history, contacts, messages); INI via QSettings (config) | Zero services |
| Secrets | QtKeychain (Secret Service). With no Secret Service, Tincan asks before storing a password in the 0600 config file | Passwords stay out of config files by default |
| Build | CMake + Ninja; PJSIP built from a pinned, SHA-256-verified release tarball | Debian/Ubuntu ship no usable `libpjproject-dev` |
| Tests | QtTest unit tests; headless end-to-end test against a `pjsua` peer | Real SIP on loopback, no mocks of PJSIP |

Licences: PJSIP is GPL-2.0-or-later, Qt is LGPL-3.0, QtKeychain is
BSD-2-Clause, Opus is BSD. All are compatible with AGPL-3.0-or-later.

## Feature scope

### v1.0 (the MicroSIP-class core)
- Multiple SIP accounts, one default. Registrar, proxy, auth user, display
  name, transport UDP/TCP/TLS, SRTP (off/optional/mandatory), STUN, ICE,
  public-address override, registration expiry, keep-alive, dial prefix,
  voicemail number.
- Dialpad with DTMF (RFC 4733, SIP INFO, in-band), redial, paste-to-dial
  (number clean-up of spaces, dashes, brackets).
- Calls: answer, reject (486/603), hang up, hold/resume, mute, speaker
  volume, blind and attended transfer, several simultaneous calls, local
  3-way conference, call recording to WAV, call timer.
- Incoming: tray notification with Answer/Reject, ringtone choice, call
  waiting tone, auto-answer (always, or on `Call-Info: answer-after` /
  `Alert-Info` intercom headers), Do Not Disturb, local forwarding
  (always / busy / no answer).
- Contacts: local address book, presence and BLF via SUBSCRIBE, CSV
  import/export.
- History: dialed/received/missed, filter and search, call back.
- Messaging: SIP MESSAGE chat per contact, stored locally.
- Audio: choose capture, playback and ring devices; echo cancellation,
  noise suppression, VAD; codec enable and order (Opus, G.722, PCMU, PCMA,
  GSM, Speex).
- Desktop: system tray with status, minimise to tray, start minimised,
  autostart, single instance, `sip:`/`tel:`/`callto:` URI handler,
  command line (`tincan sip:100@pbx`, `--answer`, `--hangup`), desktop
  notifications, translations (Qt Linguist).

### Later
- Video calls, BLF busy-lamp panel, LDAP/CardDAV directory, headset HID
  buttons, global hotkeys via the XDG GlobalShortcuts portal, Flatpak.

## Architecture

Four layers. Arrows show allowed dependencies; nothing points upward.

```
ui  ──►  app  ──►  core  ◄──  sip
```

- **core/** (no Qt widgets, no PJSIP): plain value types and interfaces.
  `AccountConfig`, `CallInfo`, `CallState`, `Contact`, `HistoryEntry`,
  `ChatMessage`, `PhoneNumber` normalisation, and the `ISipEngine`
  interface plus `ISipEvents` callback interface. Fully unit-testable.
- **sip/** (only layer that includes PJSIP): implements `ISipEngine` with
  pjsua2. Small single-purpose classes: `SipEndpoint` (library lifecycle
  and transports), `SipAccount`, `SipCall`, `AudioDeviceCatalog`,
  `CodecCatalog`, `CallRecorder`, `ConferenceBridge`, `PresenceBuddy`,
  `EventMarshaller`.
- **app/** (QtCore/QtSql/QtDBus, no widgets): use-case services wired by
  constructor injection. `AccountService`, `CallService`,
  `ContactRepository`, `HistoryRepository`, `MessageRepository`,
  `SettingsStore`, `SecretStore`, `RingtonePlayer`, `SingleInstance`,
  `CommandLine`, `UriParser`, `AutoAnswerPolicy`, `ForwardingPolicy`,
  `Autostart`, `Notifier` (freedesktop notifications over D-Bus),
  `ConfigSecretStore` / `FallbackSecretStore` (opt-in file fallback).
- **ui/** (QtWidgets): `MainWindow`, `DialpadWidget`, `CallPanel`,
  `IncomingCallPopup`, `ContactsView`, `HistoryView`, `ChatWindow`,
  `SettingsDialog` with one class per page, `AccountDialog`, `TrayIcon`.

`main.cpp` is the composition root: it builds every object and injects it.
There are no singletons and no globals outside PJSIP's own.

### Threading

PJSIP runs its own worker thread (`threadCnt = 1`) and calls pjsua2
callbacks on it. Those callbacks never touch Qt objects directly.
`EventMarshaller` copies callback data into core value types and posts
them to the Qt main thread with queued `QMetaObject::invokeMethod`.
All commands from the UI run on the Qt main thread. That thread calls
`Endpoint::libCreate()`, and pjlib registers the thread that initialises
it, so no explicit `libRegisterThread` call is needed. A pjsua2 `Call`
object is deleted only after its DISCONNECTED event has been delivered to
the main thread, never inside its own callback. pjsua may hand a
disconnected call's id to a new call before that happens, so a
disconnected `SipCall` stops acting on PJSIP and forgets its call id before
`pj::Call`'s destructor runs; otherwise that destructor would detach, and
could hang up, the new call.

The microphone is connected to calls and to call recordings only on the
main thread. While a call is muted, its recording gets no microphone audio,
so muted spans are silent on our side.

### Data

- Config: `$XDG_CONFIG_HOME/tincan/tincan.ini` (QSettings, INI).
- Database: `$XDG_DATA_HOME/tincan/tincan.sqlite` with a schema version
  table and forward-only migrations.
- Recordings: `$XDG_MUSIC_DIR/Tincan` by default, configurable. Each WAV is
  created owner-only (0600) before PJSIP writes audio and never overwrites
  an existing file; a directory Tincan creates for them is 0700.
- The database and its `-wal`/`-shm` files are 0600; the data directory is
  0700.
- Passwords: Secret Service via QtKeychain, keyed by account id.
- Single-instance socket: `$XDG_RUNTIME_DIR/tincan-<uid>` when that
  directory is private to the user, else `$XDG_CACHE_HOME/tincan/ipc/`
  (created 0700; refused if another user owns it or it is shared). Both
  ends check the peer uid with `SO_PEERCRED`.

## Code rules
- Files under ~400 lines (hard cap 800). Functions under ~40 lines (hard
  cap 60). One purpose per class. Checked in CI by `tools/size-check.sh`.
- `clang-format` (file in repo) and `-Wall -Wextra -Werror` in CI.
- Every new feature arrives with tests at the lowest layer that can hold
  them.

## Testing
- `tests/unit`: QtTest suites for core and app (number normalisation, URI
  parsing, auto-answer and forwarding policies, repositories on an
  in-memory SQLite, settings round-trip).
- `tests/e2e`: starts `pjsua` (built from the same PJSIP tree) on
  loopback with `--null-audio --auto-answer=200`, runs Tincan's engine
  with a null audio device, and asserts: direct-IP call connects, DTMF
  sends, hold/resume re-INVITEs, hang-up, incoming call auto-answer,
  SIP MESSAGE both ways, blind transfer.
- UI smoke test under `xvfb-run` with `QT_QPA_PLATFORM=offscreen`.

## Packaging
- `cmake --install` layout with `.desktop` file (declares
  `x-scheme-handler/sip`, `tel`, `callto`), AppStream metainfo, icons.
- Man page `tincan.1`, `THIRD_PARTY_NOTICES.md` and a machine-readable
  `copyright` file in `share/doc/tincan`.
- Release artifacts, built by `.github/workflows/release.yml` on `v*` tags,
  each in a container of the distribution it targets: a `.deb` for Debian 13
  (CPack, Qt 6.8), a `.deb` for Ubuntu 24.04 / Linux Mint 22 (Qt 6.4,
  version suffix `~ubuntu24.04`) and an AppImage built on Ubuntu 24.04
  (`packaging/appimage/build-appimage.sh`, linuxdeploy; glibc 2.39+), plus
  `SHA256SUMS`. The `.deb` dependencies on Qt plugin packages are looked up
  with dpkg at configure time (`cmake/DebianDistro.cmake`), because their
  names differ between the two distributions.
- Qt 6.4 is the minimum supported Qt version; CI tests Qt 6.4 and 6.8.
- Flatpak manifest in `packaging/flatpak/` (KDE runtime 6.8); not built in
  CI yet.

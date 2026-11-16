# Tincan implementation plan

Each phase is one feature branch off `dev`, squash-merged when its gate
passes. A phase's gate is: clean configure + build with `-Werror`, all
unit tests, the e2e suite, and `tools/size-check.sh`.

## P0 Skeleton
- `LICENSE` (AGPL-3.0 full text), `README.md` with MicroSIP credit and
  clean-room note, `CONTRIBUTING.md`, `.clang-format`, `.gitignore`.
- `cmake/Pjproject.cmake`: ExternalProject that downloads pjproject 2.17
  (SHA-256 pinned), configures it with `--disable-video --enable-shared=no
  CFLAGS=-fPIC --with-external-srtp --with-external-opus --with-external-speex`,
  builds it, and exposes an imported `pjproject::pjsua2` target via
  pkg-config output. Also builds the `pjsua` CLI used by e2e tests.
- Top-level `CMakeLists.txt` with targets `tincan_core`, `tincan_sip`,
  `tincan_app`, `tincan_ui`, executable `tincan`, and `tests`.
- Empty main window that starts and quits; `--version`.
- `tools/size-check.sh` and `.github/workflows/ci.yml` (Debian trixie
  container: deps, build, tests, size check).

## P1 SIP engine
- core: `AccountConfig`, `CallInfo`, `CallState`, `ISipEngine`, `ISipEvents`.
- sip: `SipEndpoint`, `SipAccount`, `SipCall`, `EventMarshaller`,
  null-audio option.
- Register an account; place, receive, answer, reject, hang up calls.
- e2e: direct-IP call to `pjsua` peer connects and hangs up; incoming
  call from the peer is reported and answered.

## P2 Phone UI
- `MainWindow` (compact, tabs: Dial, Contacts, History, Messages),
  `DialpadWidget`, `CallPanel`, `IncomingCallPopup`, `TrayIcon`.
- `SettingsDialog` pages: Accounts (list + `AccountDialog`), Audio
  (devices, AEC, codecs), General (tray, autostart, ringtone).
- `SettingsStore`, `SecretStore`, `RingtonePlayer`.
- UI smoke test (offscreen).

## P3 Call features
- Hold/resume, mute, DTMF (3 methods), blind + attended transfer,
  several calls with switching, local conference, recording,
  auto-answer policy, DND, local forwarding, call waiting tone.
- e2e for DTMF, hold, transfer.

## P4 Contacts, history, presence, messaging
- SQLite repositories with migrations; contacts CRUD + CSV import/export;
  history view with search; presence/BLF subscriptions; SIP MESSAGE chat.
- e2e for MESSAGE both ways and presence notify.

## P5 Desktop integration
- `SingleInstance` (QLocalServer) forwarding command lines;
  `CommandLine` + `UriParser` for `sip:`/`tel:`/`callto:`; `.desktop`
  with scheme handlers; desktop notifications; autostart entry;
  Qt Linguist setup with an English source catalogue.

## P6 Packaging and release
- Install rules, AppStream metainfo, icons (original SVG).
- AppImage and `.deb` built in GitHub Actions on tag; release notes.

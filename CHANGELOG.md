# Changelog

All notable changes to Tincan are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.3] - 2026-11-21

### Added

- Keypad tones: pressing a digit on the dialpad, the in-call keypad or the
  keyboard plays that key's DTMF dual tone (120 ms) on the speaker. During a
  call DTMF is sent to the far end as before; the local tone is routed to
  the playback device only, so the caller does not hear it and it is not
  recorded. Turn it off with "Play keypad tones" in Settings > Calls (on by
  default).

## [0.1.2] - 2026-11-21

Packages for Ubuntu 24.04 and Linux Mint 22.

### Added

- Ubuntu 24.04 / Linux Mint 22 package,
  `tincan_0.1.2_ubuntu24.04_amd64.deb`, built on Ubuntu 24.04 and attached
  to each release next to the Debian 13 package. Its package version,
  `0.1.2~ubuntu24.04`, sorts below the plain `0.1.2`.

### Changed

- The AppImage is built on Ubuntu 24.04 and now runs on glibc 2.39 or newer
  (it needed glibc 2.41 before).
- Qt 6.4 is the minimum supported Qt version; Tincan builds and its tests
  pass on Qt 6.4 (Ubuntu 24.04) and Qt 6.8 (Debian 13).
- The run-time Qt plugin packages of the `.deb` (SQLite driver, SVG icon
  engine, platform plugins) are found from the build machine's Qt
  installation instead of being hard-coded Debian package names.

## [0.1.1] - 2026-11-18

Security and reliability fixes from a second review of 0.1.0.

### Security

- Call recordings are created owner-only (0600) before any audio is written,
  and a recordings directory that Tincan creates is 0700. A recording never
  overwrites or follows an existing file or symlink; a numeric suffix is
  added instead, and long caller names are shortened in file names.
- The database (history, contacts, messages) and its `-wal`/`-shm` files are
  owner-only (0600) and Tincan's data directory is 0700; files from 0.1.0 are
  tightened on start-up.
- Desktop notification bodies are escaped for notification servers that
  render markup, so a caller's display name or a message cannot inject links
  or images.
- Tincan refuses to open a database written by a newer version instead of
  running old code against a newer schema. Database open failures are logged.
- `sip:`/`tel:`/`callto:` links longer than 2048 characters are ignored.
- CI and release workflows pin third-party actions to commit SHAs, default
  to a read-only token, and do not persist checkout credentials.
- The single-instance socket no longer falls back to a shared directory such
  as `/tmp` when `XDG_RUNTIME_DIR` is unset. It now uses a 0700 directory
  under the user's cache directory, both ends verify the peer's user id, and
  concurrent start-ups no longer race to replace each other's socket.
- A failed or cancelled keychain unlock no longer offers to store the
  password in the config file; only a missing Secret Service does.

### Fixed

- Deleting a finished call can no longer detach or hang up a new call that
  reused its call id. The bundled PJSIP build now allows 32 calls (was 4);
  rebuild it with `tools/build-pjproject.sh --force`.
- Call recordings no longer capture the microphone while the call is muted.
- Accounts cannot be edited or removed while they have calls in progress.

## [0.1.0] - 2026-11-16

First public release.

### Added

- **Accounts**: several SIP accounts with one default; registrar, outbound
  proxy, auth user, display name, UDP/TCP/TLS transport, SRTP (off, optional,
  mandatory), STUN, ICE, public-address override, registration expiry, dial
  prefix and voicemail number. Passwords are kept in the Secret Service via
  QtKeychain; without one, Tincan asks before using a private config file.
- **Calls**: dial pad with paste-to-dial clean-up and redial; answer, reject,
  hang up, hold/resume, mute, per-call speaker volume, DTMF (RFC 4733, SIP
  INFO or in-band), blind and attended transfer, several simultaneous calls,
  local three-way conference, call recording to WAV and a call timer.
- **Incoming calls**: in-app popup and/or desktop notification with Answer and
  Reject, ringtone choice, call-waiting tone, auto-answer (always or on
  intercom headers), Do Not Disturb and local forwarding (always, busy, no
  answer).
- **Contacts and history**: local address book with CSV import/export,
  presence and BLF through SUBSCRIBE, own presence publishing, call history
  with missed-call badge, filter and call back.
- **Messaging**: SIP MESSAGE chat per contact, stored locally, with unread
  badges.
- **Audio**: capture, playback and ring device selection, echo cancellation
  and codec enable/order (Opus, G.722, PCMU, PCMA, GSM, Speex and more).
- **Desktop integration**: system tray, minimise/close to tray, start
  minimised, autostart, single instance, `sip:`/`sips:`/`tel:`/`callto:`
  URI handler (links fill the dial field; they never call on their own),
  command line (`--answer`, `--hangup`, `--show`, `--quit`,
  `--minimized`), freedesktop notifications and Qt Linguist translations
  (Brazilian Portuguese catalogue started).
- **Polish**: only one keypad is visible during a call (the tabs collapse
  while the in-call keypad is open), the window remembers its size, position
  and last tab, and an About dialog shows versions, licence, source link and
  credits.
- **Packaging**: `.deb` (CPack), AppImage (linuxdeploy) and a Flatpak
  manifest; man page `tincan(1)`; third-party notices and a machine-readable
  copyright file; release workflow that publishes the packages with
  `SHA256SUMS`.

[Unreleased]: https://github.com/martinsaul/tincan/compare/v0.1.3...HEAD
[0.1.3]: https://github.com/martinsaul/tincan/compare/v0.1.2...v0.1.3
[0.1.2]: https://github.com/martinsaul/tincan/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/martinsaul/tincan/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/martinsaul/tincan/releases/tag/v0.1.0

# Contributing to Tincan

Thank you for your interest in contributing to Tincan. Please follow the project guidelines below so the codebase remains clean, verifiable, and easy to maintain.

## Clean-room rule

Tincan uses MicroSIP purely as a high-level product reference. Contributors (human or AI) must **never** fetch, open, read, copy, paste, or port MicroSIP source code, icons, ringtones, translations, or UI strings. Design behavior from the relevant SIP RFCs, the PJSIP documentation, and your own engineering judgment.

## Architecture and layering

Tincan is organized into four strictly layered libraries (`src/<layer>`):

```text
ui  ──►  app  ──►  core  ◄──  sip
```

- `core` (`tincan_core`): Plain C++20 value types and interfaces (`Qt6::Core` only; no Qt Widgets, no PJSIP).
- `sip` (`tincan_sip`): The **only** layer permitted to include PJSIP headers; implements `core` SIP interfaces via `pjsua2`.
- `app` (`tincan_app`): Application services, repositories, and policies (`Qt6::Core`, `Qt6::Sql`, `Qt6::Network`, `Qt6Keychain`; no Qt Widgets, no PJSIP).
- `ui` (`tincan_ui`): Qt 6 Widgets presentation layer (`Qt6::Widgets`, `Qt6::Svg`).

Dependencies must never point upward. `src/main.cpp` serves as the composition root using constructor injection; do not introduce singletons or mutable globals.

## Code size and style rules

- **Single responsibility**: One clear purpose per class.
- **File length**: Keep source, script, and CMake files under **400 lines** (hard limit: **800 lines**).
- **Function length**: Keep functions under **40 lines** (hard limit: **60 NLOC**).
- **Verification**: Run `tools/size-check.sh` and format C++ code with `.clang-format` (LLVM base, 4-space indent, 100-column limit).
- **Warnings**: All code must compile cleanly with `-DTINCAN_WERROR=ON` (`-Wall -Wextra -Wpedantic -Werror`).
- **Headers**: Every source file must begin with:
  ```cpp
  // SPDX-License-Identifier: AGPL-3.0-or-later
  // Copyright (C) 2026 Tincan contributors
  ```

## Tests

Automated tests are required for every new feature and bug fix, placed at the lowest layer that can exercise the behavior:
- `tests/unit`: Fast QtTest unit suites for `core`, `sip`, and `app`.
- `tests/e2e`: Headless loopback tests against the `pjsua` CLI peer.

Run the test suite before submitting changes:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

## Licensing and sign-off

By contributing to Tincan, you agree that your contributions are licensed under `AGPL-3.0-or-later`. A DCO-style `Signed-off-by` trailer is **not** required on commits.

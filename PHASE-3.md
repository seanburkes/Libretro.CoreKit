# Phase 3: Software Sample

**Status:** Complete. The contentless NativeAOT sample exercises video, audio,
RetroPad input, runtime core options, reset, state, and save memory through the
reusable host.

## Goal

Phase 3 proves the reusable host with deterministic software-generated output,
without adding content parsing or an emulator. Linux x64 remains the only
RetroArch-supported target.

## Sample behavior

- Generates a moving 160x144 XRGB8888 test pattern with a RetroPad-controlled
  cursor.
- Submits 800 deterministic stereo frames per video frame at 48 kHz. RetroPad A
  increases the tone amplitude.
- Supports deterministic reset and exact 88-byte serialized-state round trips.
- Exposes a stable 64-byte save-memory region.
- Performs no managed allocation in steady-state frame execution.

The sample exposes two core options:

| Key | Values | Runtime effect |
| --- | --- | --- |
| `corekit_probe_tone` | `on`, `off` | Enables or silences generated audio. |
| `corekit_probe_palette` | `color`, `monochrome` | Selects the generated video palette. |

## Decisions

- Initial option values are read during content load. Later values are read
  only after the frontend reports `GET_VARIABLE_UPDATE`; waiting for that flag
  before the initial read loses persisted RetroArch settings.
- Core options are frontend configuration, not emulator state. Reset and state
  loading preserve the current option values, while logical deinitialization
  restores defaults for the next frontend lifecycle.
- Option parsing compares borrowed null-terminated UTF-8 spans directly. The
  frame path uses preallocated video and audio arrays, including the silent
  audio path.
- The independent C host changes both options while content is running and
  checks the resulting audio samples and XRGB8888 pixels. The RetroArch gate
  preloads non-default settings and verifies that the core applies them.
- Software rendering remains deliberate. Adding a hardware context here would
  mostly provide a more elaborate place for unrelated frontend failures to
  hide.

## Current evidence

- .NET SDK 10.0.110 Release NativeAOT build and the independent C-host lifecycle
  pass on Linux x64.
- The C oracle validates both core-option definitions, initial reads, live
  updates, output changes, reset determinism, state round trips, and the
  allocation tripwire.
- Linux x64 ASan/UBSan host with leak detection disabled for the
  process-lifetime NativeAOT runtime: 1,000 loads, 2,000 managed sessions, and
  1.40 MiB RSS growth with no sanitizer diagnostic.
- Pinned RetroArch `7bc72e8735`: 50 managed/control core switches, persisted
  `off` tone and `monochrome` palette application, save and state process
  reopens, normal frontend exits, and 7.61 MiB peak RSS growth under the 16 MiB
  ceiling.

Phase 4 is next. A CHIP-8 core will add bounded content loading and realistic
emulator state, where the reusable API can finally be judged by something less
cooperative than generated color bars.

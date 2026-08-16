# Phase 1: Canonical ABI Layer

## Goal

Phase 1 extracts the Phase 0-proven libretro ABI into a reusable, trimming-safe
managed library without moving native exports out of concrete NativeAOT core
projects.

The phase is complete when the mandatory ABI declarations and the selected
environment commands are typed, independently checked against the pinned C
header, and exercised through the NativeAOT probe on every native matrix
target. Linux x64 remains the only blocking RetroArch platform until equivalent
frontend gates exist elsewhere.

## Decisions

- `Libretro.Core` owns public blittable ABI types, constants, enums, and typed
  callback wrappers. It is not packable while the API is still being proven.
- Each concrete NativeAOT publishing project owns its 25 `retro_*` export
  methods. NativeAOT does not export `UnmanagedCallersOnly` methods from a
  referenced assembly.
- The ABI layer currently supports the project's 64-bit desktop targets. It
  rejects other pointer widths explicitly rather than guessing layouts.
- Environment callbacks are synchronous. Wrapper methods may pass stack values
  only for the duration of the callback and never retain frontend pointers.
- Environment commands are added only with a native-host assertion. Unsupported
  commands remain absent instead of acquiring optimistic placeholder APIs.
- Variadic libretro logging remains disabled until a small C `"%s"` shim is
  independently tested on each ABI.

## Completed slice: reusable foundation

- [x] Create `src/Libretro.Core` with NativeAOT and trimming analyzers enabled.
- [x] Move the proven system-info, AV-info, geometry, timing, and game-info
      layouts into the reusable assembly.
- [x] Add the currently required canonical enums and constants.
- [x] Move the managed layout guard beside the public ABI declarations.
- [x] Add typed wrappers for `SET_SUPPORT_NO_GAME` and `SET_PIXEL_FORMAT`.
- [x] Make the NativeAOT probe consume the reusable project while retaining its
      local export façade.
- [x] Verify successful and rejected pixel-format negotiation through the C
      host.
- [x] Retain the upstream libretro license notice beside the derived ABI files.

## Next slices

1. Add input descriptors and input-bitmask negotiation, including single-button
   fallback tests in the C host.
2. Add typed, non-owning directory queries for system, save, content, and assets
   paths, with explicit UTF-8 pointer-lifetime rules.
3. Add messages, language, audio/video-enable state, and fast-forward state.
4. Add core-options-v2 definitions and update polling as one vertical slice.
5. Add logging only through the audited variadic C shim described above.

Core options are intentionally later than input and directory queries because
they introduce retained nested string arrays and substantially more ownership
surface. A giant binding dump would be faster to type and slower to trust.

## Open promotion gates

- Add equivalent RetroArch lifecycle automation before claiming Windows,
  macOS, or Arm64 frontend support.
- Establish an intentional minimum glibc baseline before distributing Linux
  artifacts.
- Decide the public packaging and compatibility policy only after both the
  software sample and CHIP-8 consume the API.
- Retest `NODELETE` and bounded process memory whenever the .NET SDK or pinned
  RetroArch baseline changes.

## Current evidence

- Release solution build: zero warnings and errors.
- Linux x64 ASan/UBSan host: 1,000 loads, 2,000 managed sessions, 12,000
  checked frames, and 1.78 MiB RSS growth.
- Linux x64 RetroArch: recovery scenarios, 50 managed/control switches, normal
  frontend exit, and 3.07 MiB peak RSS growth.

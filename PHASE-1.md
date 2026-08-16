# Phase 1: Canonical ABI Layer

**Status:** Complete. Phase 2 is the next implementation phase.

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
- Input descriptors and their UTF-8 strings are core-owned and remain valid
  through `retro_unload_game`. Message and core-option setters are synchronous;
  the frontend must copy their nested data as required by `libretro.h`.
- Directory and core-option value pointers returned by the frontend are borrowed,
  read-only UTF-8 data. Callers must copy them before retaining them beyond the
  frontend-defined lifetime.
- Environment commands are added only with a native-host assertion. Unsupported
  commands remain absent instead of acquiring optimistic placeholder APIs.
- Variadic libretro logging remains disabled until a small C `"%s"` shim is
  independently tested on each ABI.

## Completed scope

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
- [x] Move the six mandatory frontend callback signatures into a reusable,
      blittable callback table.
- [x] Add input descriptors and input-bitmask negotiation, including the
      single-button fallback path.
- [x] Add typed, non-owning system, save, content, and core-assets directory
      queries. `GET_CONTENT_DIRECTORY` is preserved as the canonical obsolete
      alias of `GET_CORE_ASSETS_DIRECTORY`; both have command value 30.
- [x] Add language, audio/video-enable, and fast-forward queries with the
      defaults required when a frontend rejects each command.
- [x] Add extended messages with legacy `SET_MESSAGE` fallback.
- [x] Add core-options-v2 categories, definitions, fixed 128-value storage,
      registration, update polling, and value lookup.
- [x] Extend the managed and C layout guards for every new ABI type.
- [x] Exercise accepted and rejected optional interfaces through paired native
      host sessions while retaining the steady-state allocation tripwire.
- [x] Keep logging explicitly disabled. Calling a C variadic function through a
      made-up fixed C# signature is not an implementation strategy.

## Deliberate limits

- Only the environment commands listed above are public. Other commands are
  unsupported and have no placeholder wrapper; a missing frontend callback or
  rejected typed command returns `false` without retaining output pointers.
- Core options v2 is the selected initial option interface. Version 0/1 option
  definition fallbacks are deferred until a supported frontend requires them.
- Variadic logging has no managed entry point. A future implementation requires
  a small native `"%s"` shim and independent ABI tests on every target.

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
- The independent C host checks all new layouts against the pinned
  `libretro.h`, validates every typed command, and runs accepted/rejected
  optional-interface sessions.
- Linux x64 ASan/UBSan host: 1,000 loads, 2,000 managed sessions, 12,000
  checked frames, and 1.77 MiB RSS growth under the Phase 1 command surface.
- Linux x64 RetroArch: recovery scenarios, 50 managed/control switches, normal
  frontend exit, and 10.56 MiB peak RSS growth against the 16 MiB ceiling.
- The pull-request gate publishes and executes the same native oracle on Linux,
  Windows, and macOS across x64 and Arm64. Linux x64 remains the only target with
  the additional real-RetroArch gate.

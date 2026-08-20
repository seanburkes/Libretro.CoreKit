# Phase 2: Reusable Core Host

**Status:** In progress. The first two Linux slices are complete: lifecycle and
frame services, followed by serialized state and stable memory regions.

## Goal

Phase 2 turns the reusable ABI declarations into a managed host for one
process-global libretro core. Linux x64 remains the only RetroArch-supported
target. Other build-matrix targets continue to provide NativeAOT artifact and
independent C-host evidence only.

The phase is complete when a core implementation can use the host for the full
frontend lifecycle, software frames, RetroPad input, stereo audio, serialized
state, and stable memory regions without exposing raw frontend pointers to the
core implementation.

## Decisions

- `LibretroHost<TCore>` owns callback registration, the strict lifecycle state,
  and the failure latch. One host represents the process-global libretro state
  machine.
- The legal lifecycle is `Uninitialized -> Initialized -> ContentLoaded`, then
  back through content unload and logical deinitialization. Calls made out of
  order return a safe default or do nothing.
- The environment callback is required for initialization. Video, audio, and
  input callbacks are frame requirements: a missing callback makes `retro_run`
  a no-op without poisoning the host. RetroArch registers these after content
  load, because naturally the real lifecycle is more useful than the obvious
  guess. A managed exception latches failure until `retro_deinit` completes
  logical teardown.
- Content buffers, paths, and metadata are borrowed only for the content-load
  call. They are exposed as spans and must be copied by a core that needs them
  later.
- Video and audio spans are pinned only while their frontend callback runs. A
  core must keep ownership of the backing buffers and the frontend must not
  retain those callback pointers.
- RetroPad bitmask input is preferred. The fallback reads all 16 standard
  buttons so the reusable host does not quietly invent a three-button gamepad.
- Batched stereo audio is preferred, with `retro_audio_sample_t` as the required
  fallback. Frontend audio/video-disable flags suppress callback submission.
- Linux logging uses an audited native helper that invokes the variadic frontend
  callback with the constant format `"%s"`. The helper is linked directly and
  kept out of the public `retro_*` export surface.
- Log messages are caller-owned, null-terminated UTF-8 spans with no embedded
  null. This keeps the frame path allocation-free and format-string-free.
- Logging is enabled only in Linux NativeAOT publishing projects for now. The
  managed wrapper discards a callback when the frontend rejects
  `GET_LOG_INTERFACE`; other platforms remain outside the frontend support
  claim.
- Native exports stay in each concrete publishing assembly and forward into the
  reusable host. NativeAOT still does not export `UnmanagedCallersOnly` methods
  from a referenced assembly, because apparently one runtime limitation per
  phase would have been too tidy.
- A core reports one nonnegative serialized-state size after successful content
  load. The host caches it for that loaded session, requires exact-size caller
  buffers, and returns `false` for null or mismatched buffers. A zero size means
  serialized state is unsupported.
- A core validates serialized state completely before mutating live state. The
  host contains exceptions at the native boundary, but it cannot un-corrupt a
  half-applied state for a core that ignored this fairly fundamental rule.
- A core owns each `Memory<byte>` region it advertises. The host pins save RAM,
  RTC, system RAM, and video RAM after content load succeeds and releases the
  pins only after content unload. Empty regions remain null/zero; ROM and
  unknown region identifiers are not exposed by this slice.

## Completed first slice

- [x] Add `ILibretroCore` and typed environment, initialization, content-load,
      and frame contexts.
- [x] Add lifecycle state enforcement and phase-correct callback validation.
- [x] Contain core exceptions, latch failure, and always complete logical
      teardown.
- [x] Reject malformed content buffers before constructing managed spans.
- [x] Add full RetroPad polling with bitmask and per-button paths.
- [x] Add validated XRGB8888 video submission.
- [x] Add batched signed 16-bit interleaved stereo submission with sample
      fallback.
- [x] Honor frontend audio/video-enable and fast-forward state each frame.
- [x] Keep the frame path allocation-free and retain the allocation tripwire.
- [x] Add the typed logging environment command and Linux `"%s"` bridge.
- [x] Migrate the NativeAOT probe to the reusable host.
- [x] Extend the independent C host for accepted/rejected logging, late frame
      callback registration, invalid call order, malformed content, audio
      fallback, and frontend A/V suppression.

## Completed second slice

- [x] Add exact-size caller-buffer serialization and unserialization to the
      core contract and host.
- [x] Cache a stable serialized-state size for the loaded session.
- [x] Pin advertised save RAM, RTC, system RAM, and video RAM from successful
      content load through content unload.
- [x] Keep memory pointers available after a latched frame failure so the
      frontend can still preserve save data during teardown.
- [x] Add an 88-byte deterministic probe state and writable 64-byte save RAM.
- [x] Reject null, short, and malformed serialized state without mutation.
- [x] Prove memory pointer stability across reset and state load, plus
      deterministic video/audio restoration, in the independent C host.
- [x] Exercise RetroArch state framing, process-reopen state load, save-memory
      persistence, and normal frontend exit.

## Next slice

The next vertical slice is static system metadata ownership:

1. Move `retro_get_system_info` metadata from the probe-specific runtime into
   the reusable core contract and host.
2. Define process-lifetime UTF-8 ownership for library name, version, and valid
   extensions without returning movable managed pointers.
3. Forward controller-port device changes instead of leaving the concrete
   export as a stub.
4. Exercise metadata before initialization and after logical teardown through
   both the C oracle and RetroArch.

General path/data content adapters remain a separate follow-up slice. Folding
them into static metadata would combine process-lifetime and call-lifetime
ownership for no practical gain.

## Pinned frontend harness limitation

The pinned RetroArch command gate cannot synchronously `UNLOAD_CORE` to its
dummy core when the active core advertises save RAM. The transition stops
responding before `retro_unload_game`; the same behavior reproduces with the
conventional C control core, so it is scoped to the lifecycle-command harness,
not the managed host.

The loader switch process therefore uses RetroArch's documented
`noload-nosave` SRAM mode. Separate normal frontend processes exercise save
state plus save RAM followed by `QUIT`, then reopen the core, load that state,
and quit normally. The independent C host remains the ABI oracle for in-process
pointer lifetime and deterministic restoration.

## Current evidence

- .NET SDK 10.0.110 Release NativeAOT publish and the independent C-host
  lifecycle pass on Linux x64 with the Phase 2 host.
- The exported dynamic symbol list remains the canonical 25 `retro_*` entry
  points; the logging helper is hidden.
- The native host exercises accepted and rejected optional interfaces in each
  logical session, verifies RetroArch-style late frame callback registration,
  and now validates exact state buffers and stable memory pointers.
- Linux x64 ASan/UBSan host with leak detection disabled for the process-lifetime
  NativeAOT runtime: 1,000 loads, 2,000 managed sessions, and 1.39 MiB RSS
  growth with no sanitizer error.
- Pinned RetroArch `7bc72e8735`: recovery scenarios, 50 managed/control core
  switches, 64-byte save RAM, framed 88-byte state save, state load after a
  process reopen, normal frontend exits, 6.63 MiB peak RSS growth under the
  16 MiB ceiling, and expected process-lifetime module residency after logical
  unload.

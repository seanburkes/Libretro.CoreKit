# Phase 0 Playbook: Prove NativeAOT as a Libretro Core

## Purpose

Phase 0 answers one question before the reusable framework or Craterboy adapter
is built:

> Can a well-structured C# NativeAOT shared library behave reliably as a
> dynamically loaded RetroArch core across the intended desktop lifecycle?

The phase produces a real, runnable vertical slice rather than an isolated
interop experiment. At the end, a tiny C# core must publish as a native shared
library, expose the complete libretro entry-point surface, run through a native
C host, display and respond inside RetroArch, and survive repeated teardown and
reload exercises.

NativeAOT library unloading is not supported by Microsoft. This phase must
collect evidence about what actually happens in the libretro lifecycle. A
successful build or one successful RetroArch launch is not enough.

## Required Outcome

Phase 0 ends with one of two recorded decisions:

- **Go:** NativeAOT is sufficiently reliable for the explicitly tested desktop
  platforms, so Phase 1 may begin.
- **No-go:** NativeAOT fails unload, reload, switching, shutdown, or bounded
  memory criteria. Stop framework development and investigate alternatives in
  a separate decision document.

Do not soften or silently defer this decision. Unknown results on a claimed
platform mean Phase 0 is incomplete, not successful.

## Working Style: Vertical Slices

Implement Phase 0 as a sequence of small end-to-end slices. Every slice must:

1. Add one coherent behavior from C# implementation through the native ABI.
2. Exercise that behavior from the C host or RetroArch.
3. Add an automated test wherever the behavior can be automated.
4. Leave the repository building with warnings treated as errors.
5. Record any platform-specific observation that affects later design.

Prefer a focused commit per completed slice. Do not build all ABI declarations,
then all exports, then all tests as disconnected horizontal layers. The purpose
of the vertical approach is to discover calling convention, lifetime, export,
and NativeAOT problems while the implementation is still disposable.

Phase 0 should use strong C# patterns, but it must not speculate about the full
framework. Extract an abstraction only when it protects a real invariant or is
used by more than one export. Production quality here means explicit ownership,
testable lifecycle transitions, and safe native boundaries—not a large class
hierarchy.

## Scope

Phase 0 includes:

- A new .NET solution targeting the repository's selected supported .NET SDK.
- One NativeAOT C# class-library publishing project.
- One small native C host compiled against the pinned canonical `libretro.h`.
- The complete required `retro_*` export surface, with explicit safe stubs for
  functionality not exercised by the sample.
- A contentless or dummy-content software sample.
- XRGB8888 video, a simple stereo tone, and RetroPad input.
- Native symbol, ABI, lifecycle, stress, and RetroArch tests.
- Native CI jobs for Windows, Linux, and macOS.
- A written compatibility result with logs and measurements.

Phase 0 excludes:

- A public NuGet API.
- Source generation.
- Complete bindings for optional libretro interfaces.
- Core options v2, VFS, achievements, hardware rendering, or advanced input.
- Save-state implementation beyond correct unsupported stubs.
- CHIP-8 or Craterboy code.
- Optimization beyond eliminating obvious per-frame allocations.
- General cross-platform claims beyond the tested matrix.

## Architecture for the Probe

Use a small architecture that can survive into later phases without pretending
to be the final framework:

```text
src/
  Libretro.NativeAot.Probe/
    Abi/
      LibretroTypes.cs
      LibretroConstants.cs
    Core/
      ProbeCore.cs
      CoreLifecycle.cs
      CallbackTable.cs
    Native/
      LibretroExports.cs
      ExportBoundary.cs
    Libretro.NativeAot.Probe.csproj
tests/
  Libretro.NativeAot.Probe.Tests/
  Libretro.NativeHost/
    include/libretro.h
    native_host.c
    CMakeLists.txt
artifacts/
  phase-0/                    # ignored local evidence; CI uploads artifacts
eng/
  libretro/
    libretro.h
    VERSION
    SHA256
docs/
  phase-0-results.md
```

The exact names may change, but preserve the responsibilities:

- `Abi` contains only blittable native representations and constants.
- `Core` owns managed state and sample behavior without native export attributes.
- `Native` contains the unsafe C boundary and no sample rendering logic.
- `NativeHost` is an independent C consumer and ABI oracle.

The C# publishing project must contain `LibretroExports`. NativeAOT does not
export `UnmanagedCallersOnly` methods found only in referenced assemblies.

## C# Design Rules

### One explicit state owner

Represent the libretro singleton with one internal runtime owner. A suitable
shape is an internal static façade holding one nullable `ProbeCore` instance and
one `CallbackTable` value. Do not scatter mutable static fields across export
methods.

`ProbeCore` owns:

- Lifecycle state.
- Frame number and input state.
- Preallocated pixel and audio arrays.
- Any pinned UTF-8 metadata.
- Whether dummy content is loaded.
- Error/fatal state needed for safe fallback behavior.

The instance must be created or reset deliberately by `retro_init` and disposed
by `retro_deinit`. Do not depend on the operating system re-running static
constructors after unload.

### Explicit lifecycle state machine

Use a small enum such as:

```text
Uninitialized -> Initialized -> ContentLoaded -> Initialized -> Uninitialized
```

Allow `retro_get_system_info` and callback registration before initialization,
as required by libretro. Define the permitted state for every other export.
Invalid ordering must return a safe value or become a no-op; it must never
dereference missing callbacks or throw across the boundary.

Test normal transitions, repeated teardown, and invalid ordering separately.

### Thin native exports

Every export should do only three things:

1. Validate or translate unmanaged arguments.
2. Enter a common exception boundary.
3. Forward to the managed state owner.

Keep rendering, tone generation, input mapping, and lifecycle policy out of the
export class.

Each export must be:

- `static`.
- Marked with `UnmanagedCallersOnly`.
- Given the exact `retro_*` entry-point name.
- Cdecl where the target ABI requires it.
- Limited to unmanaged arguments and return values.

### Exception containment

No managed exception may cross an unmanaged callback or export boundary.

Define explicit fallback behavior by return category:

- `void`: record the failure and return.
- C `bool`: return `0`.
- pointer: return null.
- size/count: return zero.
- `retro_api_version`: return the compile-time API constant without executing
  fallible code.

The common boundary may reduce repetition, but avoid reflection and avoid a
generic design that causes unclear NativeAOT roots. The generated machine code
and failure result must remain straightforward to audit.

Since variadic libretro logging is outside Phase 0, capture the first managed
failure in an internal fixed-size diagnostic record. The C host may retrieve
diagnostic status only through test-specific behavior compiled into probe
builds; do not add nonstandard exports to release-shaped builds without clearly
marking them.

### ABI types

Use:

- `byte` for C `bool` in exports and structs.
- `nuint` for `size_t`.
- `uint`, `short`, `ushort`, `long`, and `ulong` for matching fixed native
  widths.
- `void*` and typed pointers where the header specifies pointers.
- `delegate* unmanaged[Cdecl]<...>` for frontend callbacks.
- Explicit sequential layout for every struct.

Do not use managed `bool`, `string`, arrays, delegates, or reference types in an
unmanaged signature.

### Strings and pointer lifetimes

Libretro metadata uses null-terminated UTF-8 pointers that the frontend may
retain. Store these in stable pinned or unmanaged memory for the lifetime of the
core library. Do not use `CharSet.Auto`, temporary stack buffers, or pointers
obtained from movable arrays.

Recommended Phase 0 pattern:

- Create known ASCII/UTF-8 metadata as static byte data with a terminating zero.
- Copy it once into pinned arrays or explicit unmanaged allocations.
- Make metadata available before `retro_init`, because
  `retro_get_system_info` may be called first.
- Release only resources whose lifetime contract permits release; explicitly
  document process-lifetime metadata if that is the safest probe behavior.

### Frame and audio buffers

Allocate frame and audio arrays once per managed core instance.

- Use a 160x144 XRGB8888 frame to keep the path relevant to Craterboy.
- Pin the frame only for the duration of the synchronous video callback unless
  the buffer itself is deliberately allocated in stable pinned storage.
- Generate a fixed number of interleaved `short` stereo samples per frame.
- Pin the audio buffer only during the batch callback.
- Do not allocate, resize, format strings, use LINQ, or create exceptions in the
  normal `retro_run` path.

The frontend callbacks are invoked synchronously and must not be retained by
the probe after replacement or deinitialization.

### Callback ownership

Store frontend function pointers directly in a blittable `CallbackTable`.
Replacing a callback replaces the pointer; there is no managed delegate to root.

Before invoking a callback:

- Confirm the pointer is non-null.
- Confirm the lifecycle permits the operation.
- Keep all pointed-to managed data pinned for the complete call.
- Treat callback return values according to `libretro.h`.

No background thread may call a frontend callback. Phase 0 should create no
threads, timers, finalizers, or asynchronous work.

## Required Export Surface

Export every standard libretro core entry point even when Phase 0 implements
only a safe stub:

```text
retro_set_environment
retro_set_video_refresh
retro_set_audio_sample
retro_set_audio_sample_batch
retro_set_input_poll
retro_set_input_state
retro_init
retro_deinit
retro_api_version
retro_get_system_info
retro_get_system_av_info
retro_set_controller_port_device
retro_reset
retro_run
retro_serialize_size
retro_serialize
retro_unserialize
retro_cheat_reset
retro_cheat_set
retro_load_game
retro_load_game_special
retro_unload_game
retro_get_region
retro_get_memory_data
retro_get_memory_size
```

Phase 0 behavior should be:

- Accept callback registration before `retro_init`.
- Return `RETRO_API_VERSION` from `retro_api_version`.
- Return stable metadata from `retro_get_system_info`.
- Return 160x144 geometry, the chosen fixed frame rate, and the chosen audio
  rate from `retro_get_system_av_info` after successful load.
- Negotiate XRGB8888 using `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT`.
- Declare support for no-content operation, or accept a documented dummy
  extension. Supporting no content is preferred for the smallest probe.
- Poll input at least once during every `retro_run`.
- Render one deterministic frame and submit one audio batch per `retro_run`.
- Reset animation, tone phase, and input-derived sample state in `retro_reset`.
- Return zero/false/null from unsupported serialization, cheat, subsystem, and
  memory functions without throwing.
- Return NTSC from `retro_get_region`.
- Release managed session resources and clear callbacks/state deliberately in
  `retro_deinit` according to the tested lifecycle contract.

Decide and document whether callback pointers survive `retro_deinit`. The safer
probe default is to clear them and require the frontend to register them again.

## Vertical Slice Checklist

### Slice 0.1: Reproducible foundation

Goal: establish a small repository that makes AOT and native-boundary mistakes
visible immediately.

- [ ] Create the solution and NativeAOT class-library publishing project.
- [ ] Pin the .NET SDK with `global.json`.
- [ ] Enable nullable references, implicit usings as desired, deterministic
      builds, unsafe blocks, and warnings as errors.
- [ ] Enable `PublishAot`, `NativeLib=Shared`, self-contained publishing, and
      trimming/AOT analyzers in the project rather than only command-line flags.
- [ ] Add repository-wide formatting and analyzer configuration.
- [ ] Select and add the permissive project license.
- [ ] Copy the canonical `libretro.h`, retaining its license header.
- [ ] Record upstream repository, commit, date, and SHA-256 in `VERSION` and
      `SHA256` files.
- [ ] Add a script or documented command that verifies the pinned hash.
- [ ] Add build commands for the local RID.
- [ ] Confirm a clean managed build has no warnings.
- [ ] Confirm the local platform has the required native compiler/linker.

Evidence:

- SDK and native toolchain version output.
- Clean build log.
- Header provenance and checksum.

Exit criterion: a clean checkout can build an empty NativeAOT shared library
using one documented command.

### Slice 0.2: One proven C export

Goal: prove that the publishing project emits an exact callable C symbol.

- [ ] Add the minimum ABI constant for `RETRO_API_VERSION`.
- [ ] Implement `retro_api_version` with `UnmanagedCallersOnly`.
- [ ] Publish the native shared library for the local RID.
- [ ] Inspect the dynamic export table and verify the undecorated symbol name.
- [ ] Create the native host's platform loader abstraction.
- [ ] Load the library, resolve `retro_api_version`, call it, and verify the
      result against the pinned header.
- [ ] Close the library handle once and record the result.

Evidence:

- Export-table output.
- Native host output showing the returned API version.

Exit criterion: a C process calls C# NativeAOT through the exact public symbol.

### Slice 0.3: Complete export shell and ABI layouts

Goal: make the probe look like a complete libretro core before adding behavior.

- [ ] Add only the structs, enums, and callback signatures required by Phase 0.
- [ ] Implement all standard exports with explicit safe stubs.
- [ ] Make the native host resolve every required symbol and fail on any
      missing symbol.
- [ ] Add a C ABI report for `sizeof`, alignment, and `offsetof` of every bound
      structure.
- [ ] Add managed layout tests that compare against the C report.
- [ ] Exercise null and unsupported paths for every stub.
- [ ] Check Windows x64 naming/calling convention explicitly when that runner is
      introduced.

Do not generate thousands of lines of unused bindings in this slice.

Exit criterion: all exports resolve and every bound layout matches the C header
on the local architecture.

### Slice 0.4: Lifecycle and callback registration

Goal: implement the process-global state machine independently of rendering.

- [ ] Add `CoreLifecycle`, `CallbackTable`, and the single state owner.
- [ ] Implement all six callback setters using unmanaged function pointers.
- [ ] Implement idempotent initialization and deinitialization.
- [ ] Implement no-content or dummy-content load/unload behavior.
- [ ] Implement reset and controller-device selection.
- [ ] Have the C host record every callback and lifecycle call.
- [ ] Test the expected frontend order.
- [ ] Test deliberately invalid orderings and verify safe results.
- [ ] Test two logical sessions without closing the native library.
- [ ] Force garbage collections between registration and invocation to prove no
      managed delegate lifetime is involved.

Exit criterion: the lifecycle can repeat in one loaded library without stale
state, exceptions, or callback corruption.

### Slice 0.5: System information and environment negotiation

Goal: exercise retained strings, caller-owned structs, and the bidirectional
environment callback.

- [ ] Add stable UTF-8 library name, version, and valid-extension storage.
- [ ] Populate `retro_system_info` correctly before initialization.
- [ ] Populate AV information after load.
- [ ] Request XRGB8888 and require a successful environment response.
- [ ] Request or declare no-content support as designed.
- [ ] Make the C host validate every returned value and pointer after forced GC.
- [ ] Verify metadata pointers remain valid through init, load, run, unload, and
      deinit for the lifetime promised by the implementation.
- [ ] Test an environment callback that rejects an optional command.

Exit criterion: both sides exchange structs and retained UTF-8 pointers without
layout or lifetime failures.

### Slice 0.6: One complete audiovisual/input frame

Goal: prove the frame loop end to end with no steady-state allocations.

- [ ] Preallocate a 160x144 XRGB8888 frame buffer.
- [ ] Render a deterministic moving pattern in managed code.
- [ ] Poll input once and query at least one RetroPad control.
- [ ] Make input visibly alter the pattern.
- [ ] Preallocate a stereo audio buffer and generate a deterministic tone.
- [ ] Submit video once and audio once per `retro_run`.
- [ ] Have the C host verify dimensions, pitch, pixel changes, audio frame
      count, channel interleaving, and input polling.
- [ ] Run enough frames to trigger multiple garbage collections.
- [ ] Add an allocation assertion or benchmark proving no managed allocations
      occur after warm-up in `retro_run`.
- [ ] Verify reset produces the original deterministic first frame and tone
      phase.

Exit criterion: the C host drives a stable, allocation-free C# frame loop with
video, audio, and input.

### Slice 0.7: Native unload/reload stress harness

Goal: characterize the unsupported NativeAOT library-unload boundary before
using RetroArch.

- [ ] Run the full logical lifecycle, close the library, reopen it, resolve new
      symbols, and repeat.
- [ ] Detect whether the module actually leaves the process address space using
      a platform-appropriate mechanism where reliable.
- [ ] Distinguish logical teardown success from physical module unloading.
- [ ] Run 10 diagnostic cycles with verbose event logging.
- [ ] Run 100 release cycles while checking state reset and output hashes.
- [ ] Run 1,000 release cycles for the platform gate.
- [ ] Record process RSS/private memory after a warm-up period and at regular
      intervals.
- [ ] Record handle, thread, and module counts where platform APIs permit it.
- [ ] Fail immediately on crash, hang, callback after teardown, stale frame
      state, or inconsistent output hash.
- [ ] Run under AddressSanitizer for the C host where supported; use platform
      leak/memory tools on Windows and macOS where practical.

Default bounded-growth criterion after warm-up:

- No positive linear memory-growth trend attributable to each reload.
- No more than 16 MiB total retained growth over 1,000 cycles.
- If platform/runtime behavior makes this threshold inappropriate, record and
  approve a replacement threshold in an ADR before judging the gate.

Exit criterion: the behavior is measured and repeatable, not merely observed
once.

### Slice 0.8: RetroArch vertical slice

Goal: prove the same binary in the reference frontend.

- [x] Create a minimal `.info` file for the probe core.
- [x] Install the core and info file into an isolated RetroArch test profile.
- [x] Start RetroArch with verbose logging and no unrelated user configuration.
- [x] Load the probe without content or with its documented dummy content.
- [ ] Verify visible animation, audible stereo output, and responsive input.
- [x] Exercise reset.
- [x] Close content and load the probe again.
- [x] Restart the probe core.
- [x] Switch from the probe to one pinned conventional core and back.
- [x] Repeat the switch sequence 50 times manually or through stable UI/CLI
      automation.
- [x] Quit RetroArch normally after the probe has run.
- [ ] Repeat after a failed content load and after an unsupported operation.
- [x] Check RetroArch logs and OS crash reports after every scenario.
- [x] Record whether the native module remains resident after core closure.
- [x] Measure process memory across the switch sequence.

Use a pinned RetroArch release for the blocking gate. A current stable release
may be tested additionally, but changing frontend behavior must not make the
evidence irreproducible.

Exit criterion: ordinary load, close, reload, switch, and exit workflows behave
reliably with bounded resource use.

### Slice 0.9: Native CI matrix

Goal: reproduce the proof on every claimed desktop platform.

- [ ] Add native jobs for Windows x64, Linux x64, macOS x64, and macOS Arm64.
- [ ] Add Arm64 Windows and Linux jobs when suitable native runners are
      available; do not emulate a passing native-runtime gate.
- [ ] Install or verify each platform's documented NativeAOT prerequisites.
- [ ] Publish independently on each operating system; do not attempt general
      cross-OS NativeAOT compilation.
- [ ] Run managed tests, native ABI tests, symbol checks, and the 1,000-cycle
      host stress test in every blocking job.
- [ ] Treat actionable trimming and AOT diagnostics as errors.
- [ ] Upload the core binary, debug symbols, C host, export report, test logs,
      memory measurements, and build manifest.
- [ ] Add RetroArch smoke automation only where it is deterministic. Retain the
      documented manual test where GUI/audio/input automation is unreliable.
- [ ] Pin runner images or record their exact OS/toolchain versions.

Exit criterion: each claimed platform produces independently verified evidence
from a native runner.

### Slice 0.10: Decision and handoff

Goal: turn evidence into a bounded architecture decision.

- [ ] Complete `docs/phase-0-results.md` using the template below.
- [ ] Classify each platform as pass, fail, or untested.
- [ ] Record whether physical unload occurred and whether it mattered.
- [ ] Record stress counts, duration, memory deltas, crashes, hangs, warnings,
      and RetroArch observations.
- [ ] List every workaround and determine whether it is acceptable architecture
      or probe-only debt.
- [ ] Confirm that no Phase 0 code depends on reflection emit, dynamic loading
      of managed assemblies, marshalled delegates, or steady-state allocation.
- [ ] Conduct a focused review of native signatures, ownership, and exception
      containment.
- [ ] Make and record the go/no-go decision.
- [ ] If go, identify only the code that should be promoted into Phase 1.
- [ ] If no-go, preserve the probe and evidence and stop feature development.

Exit criterion: a reviewer can reproduce the decision from repository evidence
without relying on oral context.

## Native Host Requirements

The native host should remain small, strict, and independent from the managed
implementation.

It must:

- Compile as C against the exact pinned `libretro.h`.
- Load libraries with `LoadLibrary`/`GetProcAddress` on Windows and
  `dlopen`/`dlsym` on Unix-like systems.
- Treat every required missing symbol as a hard failure.
- Register C callbacks for environment, video, audio, input poll, and input
  state.
- Copy or hash callback data synchronously without retaining transient frame or
  audio pointers.
- Validate lifecycle event order and callback counts.
- Support one verbose diagnostic iteration and quiet stress iterations.
- Enforce timeouts so a hanging unload or deinit fails CI.
- Emit machine-readable results in addition to concise human output.
- Return a nonzero exit code for every failed invariant.

Keep platform loading differences behind a very small C interface. Do not use a
C++ test framework or another managed runtime, because the host is meant to be
an independent consumer of the C ABI.

## Test Inventory

At minimum, create these tests:

### Managed tests

- Lifecycle transition table.
- Repeated init/deinit behavior.
- Reset determinism.
- Pattern and tone determinism.
- Input-to-state mapping.
- Error recording and fallback values.
- No allocations in warmed `retro_run` implementation code.

Managed tests should target the core/state owner directly. Do not call
`UnmanagedCallersOnly` methods from managed code.

### ABI tests

- All required exports exist.
- `retro_api_version` returns the header constant.
- Every bound struct matches C size, alignment, and offsets.
- Every callback signature can be invoked correctly.
- C `bool` values round-trip as one-byte values.
- `size_t` values use the native pointer width.
- UTF-8 metadata is correct and remains readable for its promised lifetime.

### Lifecycle tests

- Callback registration before initialization.
- System info before initialization.
- Init/load/run/unload/deinit happy path.
- No-content load.
- Repeated reset.
- Repeated content load/unload without library closure.
- Deinit after partial setup.
- Duplicate deinit.
- Missing callbacks.
- Rejected pixel-format negotiation.
- Unsupported serialization, cheats, subsystems, and memory access.

### Stress tests

- Long run in one loaded library.
- Repeated logical sessions in one loaded library.
- Repeated native close/reopen cycles.
- Forced GC before and during run loops.
- State/output hash comparison across reloads.
- Memory, thread, handle, and module-count sampling.

### RetroArch tests

- Load and visible/audible run.
- Input response.
- Reset.
- Close and reload content.
- Restart core.
- Switch to another core and back.
- Repeated switching.
- Normal frontend exit.
- Recovery from rejected content/environment negotiation.

## `phase-0-results.md` Template

Use a table for the result matrix:

```text
| Platform | Architecture | OS | .NET SDK | RetroArch | Host stress |
| Physical unload | Memory delta | Result |
```

For each platform include:

1. Build runner and native toolchain versions.
2. Published RID and binary checksum.
3. Export verification output.
4. ABI layout-test result.
5. Logical lifecycle-test result.
6. Physical unload observation and detection method.
7. Stress iteration count, duration, and memory samples.
8. RetroArch scenarios and logs.
9. Known warnings or deviations.
10. Pass/fail rationale.

End the document with:

- Decision: go or no-go.
- Exact supported platform claim.
- Accepted limitations.
- Required Phase 1 follow-ups.
- Approver/reviewer and date.

## Go Criteria

Phase 0 passes only when all of the following are true for every claimed
blocking platform:

- The core publishes without unexplained trimming or AOT warnings.
- All required symbols are exported with the correct names.
- Every bound ABI layout matches the C header.
- The C host completes the lifecycle and validates video, audio, and input.
- The warmed frame path performs no steady-state managed allocations.
- The 1,000-cycle load/lifecycle/close/reopen stress test completes without a
  crash, hang, stale state, callback-after-teardown, or output inconsistency.
- Resource use is bounded by the recorded threshold.
- RetroArch can load, run, reset, close, reload, switch away, switch back, and
  exit reliably.
- Any failure remains contained inside the native boundary.
- The exact evidence and tool versions are preserved.

Physical unloading is not itself required if the platform loader deliberately
keeps one stable module instance resident and all normal workflows remain safe
with bounded memory. That behavior must be documented as a limitation and
retested whenever the .NET SDK or supported RetroArch baseline changes.

## No-Go Criteria

Stop and reassess if any blocking platform exhibits:

- A crash or hang during unload, reload, switching, or frontend exit.
- Frontend callbacks after logical deinitialization.
- Stale managed state after a new logical session.
- Missing or decorated exports that cannot be fixed with supported tooling.
- ABI layout behavior that requires unsafe platform guesses.
- Unbounded memory, thread, handle, or module growth.
- A required workaround based on undefined calling behavior.
- A NativeAOT restriction that prevents the complete libretro lifecycle.
- A solution that requires shipping CoreCLR or Mono despite the NativeAOT goal.

If only one platform fails, either remove it from the initial supported matrix
with an explicit product decision or declare Phase 0 a no-go. Do not label an
untested or failing platform supported.

## Phase 0 Completion Definition

Phase 0 is complete when:

- All ten vertical slices are complete or explicitly marked not applicable.
- The probe core and native host build from a clean checkout.
- Automated evidence exists for every blocking native platform.
- RetroArch scenarios have reproducible evidence.
- The architecture review finds no unmanaged lifetime or exception leak.
- `docs/phase-0-results.md` records a clear go/no-go decision.
- The repository contains no speculative Phase 1 framework surface.

If the decision is go, Phase 1 begins by extracting the proven ABI and hosting
patterns—not by rewriting the probe from scratch. The probe remains as a
permanent compatibility and regression fixture for future .NET SDK, operating
system, architecture, and RetroArch updates.

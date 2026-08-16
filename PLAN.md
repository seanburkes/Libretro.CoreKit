# Libretro.CoreKit Plan

## Decision

Build a new, permissively licensed, desktop-first libretro framework for .NET
using NativeAOT shared libraries.

NativeAOT is the preferred implementation strategy because it can publish a
self-contained native `.dll`, `.so`, or `.dylib` with explicit C exports and no
installed .NET runtime. It preserves the managed Craterboy implementation and
matches the way desktop RetroArch loads cores better than the available
alternatives.

Phase 0 completed with a go decision for Linux x64. Linux deliberately keeps the
NativeAOT module resident and performs logical teardown because Microsoft does
not support unloading NativeAOT shared libraries. Windows, macOS, and Arm64
native artifacts pass the independent C host but are not RetroArch-supported
until their equivalent frontend lifecycle gates pass.

## Scope

The current supported target is:

- Linux x64 with the documented process-lifetime resident module.

The intended expansion targets are:

- Windows x64 and Arm64.
- Linux x64 and Arm64 using a documented minimum glibc baseline.
- macOS x64 and Arm64.
- Dynamically loaded libretro cores.
- Software-rendered video and signed 16-bit interleaved stereo audio.
- Managed emulator implementations that are trimming- and NativeAOT-compatible.

The initial scope does not promise:

- Every platform on which RetroArch runs.
- Static core linking.
- Game consoles, iOS, tvOS, WebAssembly, or other restricted platforms.
- Hardware-rendering APIs such as OpenGL, Vulkan, Metal, or Direct3D.
- Multiple active core instances in one native library.
- A general-purpose .NET frontend capable of loading other libretro cores.

Android may be investigated after the desktop implementation is stable, but it
is not part of the first compatibility claim.

## Why Not the Alternatives

### CoreCLR hosted by a native C shim

This permits ordinary JIT-compiled assemblies but requires deployment and
resolution of the .NET runtime, `hostfxr`, runtime configuration, and managed
assemblies alongside the core. It also introduces two interacting unload
lifetimes: the native core library and the hosted runtime. This is useful as a
diagnostic fallback, not the preferred distributable core format.

### DNNE or similar native-export tooling

These tools improve native exports for ordinary .NET assemblies but do not
remove the CoreCLR deployment and lifetime issues. They may be useful for a
comparison prototype if Phase 0 exposes a NativeAOT-specific problem.

### Mono embedding or Mono AOT

This adds a separate runtime/toolchain and platform-specific embedding layer,
with more deployment complexity and no clear advantage for the desktop targets.

### A native C, C++, or Rust adapter

A native adapter cannot call the managed emulator without hosting a .NET
runtime, so it reduces to the CoreCLR/Mono alternatives. Reimplementing the
emulator in a native language would solve deployment broadly but abandons the
goal of using Craterboy and is therefore a different project.

### Experimental IL-to-C toolchains

Treat these as research projects, not a release foundation. NativeAOT is the
supported .NET path for producing self-contained native shared libraries.

## Repository and Project Structure

Create a new repository rather than deriving it from the old `Libretro.NET`
frontend implementation.

Suggested structure:

```text
eng/
  libretro/
    libretro.h
    VERSION
src/
  Libretro.Core/
    Abi/
    Environment/
    Hosting/
  Libretro.Core.Generator/       # deferred until justified
samples/
  Libretro.SoftwareSample/
  Libretro.Chip8/
tests/
  Libretro.Core.Tests/
  Libretro.Abi.Tests/
  Libretro.NativeHost/
  Libretro.RetroArch.Tests/
```

The Craterboy repository should later add a separate publishing project:

```text
src/
  Craterboy.Core/
  Craterboy.Libretro/
```

`Craterboy.Core` must remain headless, managed-only, and independent of
libretro. `Craterboy.Libretro` will reference both Craterboy and the reusable
libretro library.

## Design Principles

- Treat the canonical `libretro.h` as the ABI source of truth.
- Pin the header by upstream commit and SHA-256 hash.
- Retain its MIT license notice with generated or derived ABI declarations.
- Keep the public managed API small and strongly typed.
- Keep all unsafe code in the ABI and buffer boundary where practical.
- Use unmanaged function pointers instead of marshalled delegates.
- Allocate no managed or unmanaged memory in the steady-state frame loop.
- Never allow an exception to cross a managed/native boundary.
- Make ownership and lifetime explicit for every string, pointer, and buffer.
- Keep the core implementation independent of RetroArch-specific behavior;
  libretro is the contract and RetroArch is the reference integration target.
- Publish one native core per process-global libretro state machine.

## ABI Conventions

Use direct blittable representations:

- `byte` for C `bool` at native boundaries.
- `nuint` for `size_t`.
- Fixed-width integer types for `stdint.h` types.
- Explicit sequential layout for structures.
- `delegate* unmanaged[Cdecl]<...>` for callbacks.
- Explicit null-terminated UTF-8 strings.
- Stable unmanaged or pinned storage for memory retained by the frontend.

Do not use:

- `DispatchProxy`, `DynamicInvoke`, or reflection-based dispatch.
- `System.Reflection.Emit` or other runtime code generation.
- `Marshal.GetFunctionPointerForDelegate` in the core frame path.
- Platform-dependent `CharSet.Auto` string conversion.
- Object-layout serialization.
- Long-lived pointers into movable managed objects.

Each native export must be a static method marked with
`UnmanagedCallersOnly`, have an explicit `retro_*` entry-point name, use only
unmanaged parameter and return types, and catch all managed exceptions before
returning to the frontend.

NativeAOT only exports such methods from the assembly being published, not
ordinary referenced assemblies. Consequently, each concrete core publishing
project must contain its export façade. The reusable library should implement
the behavior behind that façade.

Hand-author the export façade initially. The libretro ABI has a small, fixed set
of mandatory exports, and explicit code is easier to review during ABI bring-up.
After both CHIP-8 and Craterboy adapters exist, evaluate a source generator that
emits forwarding exports into the consuming project. Do not make the generator
a prerequisite for the first working core.

## Phase 0: NativeAOT and RetroArch Compatibility Gate

Follow the detailed execution playbook in [PHASE-0.md](PHASE-0.md).

Create the smallest possible NativeAOT shared library exporting the mandatory
libretro symbol set. It should return static system information and produce a
software-generated frame without loading content.

Test on Windows x64, Linux x64, macOS x64, and macOS Arm64:

1. Verify exported symbol names with `dumpbin`, `nm`, or the platform equivalent.
2. Load the library with a tiny C host and resolve every mandatory symbol.
3. Execute `retro_set_*`, `retro_init`, `retro_load_game`, several
   `retro_run` calls, `retro_unload_game`, and `retro_deinit`.
4. Unload and reload the library repeatedly in the same host process.
5. Load the core in RetroArch and run it.
6. Close content, reload it, and restart the core.
7. Switch repeatedly between the NativeAOT core and a conventional core.
8. Quit RetroArch normally and check for crashes, hangs, leaked background
   threads, and corrupted state.
9. Repeat the lifecycle under AddressSanitizer or platform memory diagnostics
   where the native host permits it.
10. Record exact .NET SDK and RetroArch versions with the results.

Gate result:

- Proceed only if normal RetroArch workflows are reliable on all claimed
  platforms.
- If unloading is ignored but safe and the library remains resident, measure
  repeated-switch memory growth and define an acceptable bound before proceeding.
- If unloading crashes, hangs, corrupts state, or grows without bound, stop the
  production framework effort and compare CoreCLR/DNNE hosting with a native
  implementation strategy.

## Phase 1: Canonical ABI Layer

Follow the active slice checklist and decisions in [PHASE-1.md](PHASE-1.md).

- Pin a current canonical `libretro.h` from `libretro-common`.
- Hand-author the mandatory structs, enums, constants, callbacks, and exports.
- Represent optional environment commands incrementally rather than binding the
  entire header immediately.
- Include the complete mandatory export surface, including lifecycle,
  serialization stubs, cheats stubs, game loading, region, and memory access.
- Implement typed wrappers for the initial environment commands:
  - pixel format;
  - input descriptors;
  - input bitmasks;
  - core options v2;
  - system, save, content, and assets directories;
  - support-no-game;
  - messages;
  - language;
  - audio/video enable state;
  - fast-forward state;
  - logging, subject to the variadic-call solution below.
- Document unsupported commands and return `false` without side effects.

Libretro logging uses a C variadic callback. Do not call it through an
incorrect fixed C# signature. Implement either a tiny audited C shim that calls
the callback with a constant `"%s"` format or initially disable the logging
interface. Test this independently on every ABI.

## Phase 2: Reusable Core Host

Define a managed contract representing one core implementation. Keep libretro
concepts visible where they are semantically important, but keep raw pointers
inside the host layer.

The host must manage:

- Callback registration and validation.
- Initialization, content loading, reset, unload, and deinitialization.
- Static core metadata and audio/video timing.
- Input polling and RetroPad mapping.
- Software video buffers and pixel format negotiation.
- Batched signed 16-bit interleaved stereo audio.
- Save-state serialization into caller-provided buffers.
- Stable memory regions for save RAM, system RAM, RTC, and video RAM.
- Core options and option-update polling.
- Error capture and safe fallback behavior at every export.

Define a strict lifecycle state machine and test invalid call ordering. Ensure
`retro_deinit` releases owned resources and resets all process-global state even
when the operating system leaves the NativeAOT library loaded.

## Phase 3: Software Sample

Build a contentless software sample that exercises the host without introducing
an emulator:

- Render moving color bars or a software-rasterized triangle in XRGB8888.
- Generate a deterministic stereo tone through the batch callback.
- Move or alter the image using RetroPad input.
- Expose two core options and respond to runtime changes.
- Support reset and deterministic save-state round trips.
- Run without steady-state allocations after initialization.

Do not use OpenGL or another hardware-rendering API for this sample. Hardware
context negotiation would obscure basic ABI and lifecycle failures.

## Phase 4: CHIP-8 Reference Core

Implement or adapt a tiny dependency-free CHIP-8 emulator to exercise realistic
core behavior:

- Load content from the in-memory `retro_game_info` buffer.
- Validate content bounds and reject malformed programs safely.
- Map the keypad through RetroPad input.
- Produce deterministic video and audio.
- Implement reset, serialization, and unserialization.
- Expose core options for interpreter quirks and display behavior.
- Run deterministic replay and state-round-trip tests.

Use CHIP-8 to refine the reusable API. Do not introduce abstractions solely for
anticipated Craterboy requirements until a second implementation needs them.

## Phase 5: Craterboy Readiness Work

Complete these changes in `Craterboy.Core` before presenting it as the flagship
libretro core:

- Reach a documented playable DMG milestone with the existing differential and
  ROM-test gates passing.
- Emit preallocated interleaved stereo samples rather than the current mono
  mixed stream.
- Provide a stable, allocation-free frame access path.
- Keep raw Game Boy pixels independent from presentation palettes.
- Implement explicit, versioned state serialization and transactional loading.
- Expose stable save-RAM storage whose mutations are reflected directly in the
  cartridge, or provide a precisely synchronized adapter contract.
- Define stable system RAM, video RAM, and RTC exposure where supported.
- Preserve deterministic `RunFrame`, reset, input, time, and entropy behavior.
- Benchmark NativeAOT execution with video and audio enabled.

Do not move libretro-specific lifecycle, pointers, or environment commands into
`Craterboy.Core`.

## Phase 6: Craterboy Libretro Adapter

Create `Craterboy.Libretro` as the concrete NativeAOT publishing project.

Initial behavior:

- Accept `.gb` and `.gbc` content in memory.
- Select an appropriate Game Boy model with a core-option override.
- Report 160x144 geometry and the emulator's exact frame rate.
- Convert raw pixels into a reusable XRGB8888 buffer.
- Submit interleaved stereo samples through the audio batch callback.
- Poll input once per `retro_run` and map all Game Boy controls.
- Support reset, battery-backed RAM, and state serialization.
- Return NTSC for the libretro region value, as required for handheld systems
  without PAL/NTSC output modes.
- Advertise input descriptors and relevant core options.
- Make unsupported cheats, subsystems, and hardware rendering explicit stubs.

Later integration may add achievements-compatible memory maps, rumble, sensors,
camera support, SGB behavior, cheats, and advanced latency features when the
emulator itself supports them.

## ABI and Integration Tests

Build a small C host against the exact pinned `libretro.h`. It must be the
primary ABI oracle rather than another independently authored C# declaration.

The test suite must cover:

- Presence and exact spelling of all mandatory exports.
- C versus C# `sizeof`, alignment, and `offsetof` for every bound struct.
- Pointer width and `size_t` behavior on each architecture.
- C `bool` representation and callback return values.
- UTF-8 encoding and string lifetime.
- Callback registration and invocation order.
- Pixel format, dimensions, pitch, and frame-buffer lifetime.
- Audio interleaving, frame counts, and partial-consumption returns.
- Input polling and bitmask/single-button paths.
- Content loading from memory and full path where supported.
- Save-state buffer sizing, round trips, and malformed input rejection.
- Save-RAM pointer stability and mutation visibility.
- Repeated reset, load, unload, deinit, and reload sequences.
- Exceptions and invalid state being contained inside native exports.
- Zero steady-state allocations in the software sample and Craterboy adapter.

Add a separate RetroArch smoke suite. The small host proves the ABI; RetroArch
proves actual frontend compatibility. Pin a known RetroArch version for release
gates and periodically test the current stable release as a non-blocking
compatibility signal.

## Continuous Integration and Artifacts

Use native GitHub-hosted runners because NativeAOT does not support general
cross-OS publishing.

Initial matrix:

| Runner | RID | Artifact |
| --- | --- | --- |
| `windows-2025` | `win-x64` | `*_libretro.dll` |
| Windows Arm64 runner when available | `win-arm64` | `*_libretro.dll` |
| pinned Ubuntu baseline | `linux-x64` | `*_libretro.so` |
| Linux Arm64 runner | `linux-arm64` | `*_libretro.so` |
| Intel macOS runner | `osx-x64` | `*_libretro.dylib` |
| Apple Silicon macOS runner | `osx-arm64` | `*_libretro.dylib` |

Each job should:

1. Restore with a locked dependency graph.
2. Build with trimming and NativeAOT analyzers enabled.
3. Treat actionable trim and AOT warnings as errors.
4. Run managed unit tests.
5. Compile and run the native ABI host.
6. Publish the NativeAOT shared library.
7. Verify its exported symbols.
8. Run load/unload/reload tests.
9. Run the platform-appropriate RetroArch smoke test where automation is stable.
10. Strip release binaries while retaining separate debug symbols.
11. Produce checksums, licenses, an `.info` file, and a manifest recording the
    SDK, RID, header commit, source commit, and build options.

Use an old-enough Linux build environment to establish an intentional glibc
compatibility floor. Do not assume a binary built on the newest runner image
will work on older distributions.

## Release Gates

### Experimental

- Phase 0 passes on Linux x64, the only initially claimed frontend platform.
- The C ABI host validates every mandatory export and bound layout.
- The software sample runs in RetroArch with video, audio, and input.

### Preview

- CHIP-8 supports content, options, reset, save states, and deterministic tests.
- Both macOS architectures and Linux Arm64 are covered.
- Repeated core switching has no crash, hang, or unbounded growth.
- NativeAOT and trimming builds have no unexplained warnings.

### Stable framework 1.0

- The reusable API has been validated by at least two different cores.
- ABI declarations are reproducibly checked against the pinned header.
- Every claimed platform passes the native host and RetroArch smoke tests.
- Lifecycle, ownership, thread, encoding, and error behavior are documented.
- A compatibility policy exists for updating `libretro.h` and the .NET SDK.

### Craterboy flagship release

- Craterboy has reached its own playable/correctness milestone.
- Stereo audio, state serialization, and stable memory exposure are complete.
- Video and audio run at sustained real time without steady-state allocations.
- Battery saves and save states survive RetroArch restart and version upgrades.
- Relevant conformance ROMs and differential tests pass in the same source
  revision used to build the released libretro binaries.

## Known Risks and Mitigations

### NativeAOT library unloading

This is the primary project risk. Mitigate it only through early, repeated,
platform-specific proof. Documentation or a single successful launch is not
sufficient evidence.

### Platform coverage expectations

Libretro is much more portable than NativeAOT shared libraries. Describe the
project as desktop-first and publish an explicit platform matrix.

### ABI drift

The libretro ABI version remains stable while optional environment commands and
structs grow. Pin the header, automate layout comparisons, and update features
incrementally.

### Variadic logging

Use a tiny C shim with a fixed `"%s"` format or omit the logging interface until
it can be implemented without undefined calling behavior.

### Managed buffer movement

Use unmanaged storage, the pinned object heap, or explicit short-lived pinning
according to the frontend's pointer lifetime. Test retained memory pointers
across garbage collections.

### Global state

Libretro exposes a process-global C interface. Model one explicit host state
machine, make teardown idempotent, and never rely on the operating system to
reinitialize static fields after unload.

### Emulator incompleteness

Develop the framework against the software and CHIP-8 samples while Craterboy
continues toward playability. Do not weaken Craterboy correctness gates to make
the adapter appear complete sooner.

### Licensing

Implement from the MIT-licensed canonical header and original design work.
Retain required notices. Avoid copying the old GPL-3.0 `Libretro.NET`
implementation if the new framework is intended to use a permissive license.

## Immediate Next Actions

1. Complete the incremental environment wrappers in [PHASE-1.md](PHASE-1.md).
2. Keep every addition checked by the independent C host and native matrix.
3. Define the reusable host only after the ABI ownership rules are proven.
4. Build the software sample, then CHIP-8, before freezing a package API.
5. Add frontend platforms only after equivalent RetroArch lifecycle evidence.
6. Prepare Craterboy's stereo, serialization, and stable-memory APIs before
   creating `Craterboy.Libretro`.

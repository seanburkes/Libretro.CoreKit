# Phase 0 Compatibility Results

## Current decision

**Native-host gate: provisional go. Overall Phase 0: incomplete.**

The NativeAOT core works through the libretro ABI and an installed RetroArch,
but ordinary NativeAOT unload/reload behavior is not acceptable. Linux is only
provisionally viable when the shared library is marked `NODELETE`, keeping one
runtime resident until the frontend exits.

The native host now passes 1,000 loader cycles under a 16 MiB RSS-growth ceiling
on Linux, Windows, and macOS across x64 and Arm64. Do not extract a reusable
framework yet: the equivalent RetroArch close, reload, core-switch, and exit
tests remain outstanding.

## Probe scope

The Stage 0A artifact provides:

- all 25 mandatory `retro_*` exports with explicit Cdecl entry points;
- stable UTF-8 system metadata available before `retro_init`;
- explicit initialization, load, unload, deinitialization, and reinitialization;
- no-content and XRGB8888 environment negotiation;
- a deterministic 160x144 software frame;
- 800 interleaved stereo frames per 60 Hz video frame at 48 kHz;
- RetroPad polling on every `retro_run`;
- safe unsupported serialization, subsystem, cheat, and memory behavior;
- exception containment at every fallible native export; and
- a C11 host compiled against the pinned canonical `libretro.h`.

Callbacks are cleared by `retro_deinit`. A frontend must register them again
before starting another managed session.

## Test environment

Recorded on 2026-08-15:

| Component | Version |
| --- | --- |
| Operating system | Fedora Linux 44, x86-64, kernel 7.1.8 |
| .NET SDK | 10.0.110 |
| .NET runtime / NativeAOT toolchain | 10.0.10 |
| C compiler | GCC 16.1.1 |
| CMake | 4.3.0 |
| RetroArch | Flatpak 1.22.2, Git 69a4f0ea1e |
| `libretro.h` | `libretro-common` commit `879c8d507b0b52e77e27d759239c2b5df1e26dfd` |
| Header SHA-256 | `951c20c2e74b4e1cdfac69b702acb499902e8988e86de973d0922e23f50270ca` |

The cross-platform matrix uses `ubuntu-22.04`, `ubuntu-22.04-arm`,
`windows-2025`, `windows-11-arm`, `macos-15-intel`, and `macos-15` GitHub-hosted
runners.

## Results

`./eng/run-phase-0a.sh` completed with zero managed build warnings, compiled the
C host with warnings as errors, resolved every required export, and passed 25
load/unload cycles with two complete managed sessions per load.

The unmodified NativeAOT shared library exposed linear reload growth:

```text
PASS: 1000 load/unload cycles, 2000 managed sessions
RSS after first session: 3.80 MiB; final: 354.62 MiB; growth: 350.82 MiB
```

Keeping one library loaded while running the same number of managed sessions
did not reproduce the growth:

```text
PASS: 1 load/unload cycles, 2000 managed sessions
RSS after first session: 3.68 MiB; final: 2.76 MiB; growth: -0.93 MiB
```

The Linux build now passes `-z nodelete` to the ELF linker. `readelf` confirms
the `NODELETE` dynamic flag, and the repeated loader test becomes bounded:

```text
PASS: 1000 load/unload cycles, 2000 managed sessions
RSS after first session: 3.55 MiB; final: 4.32 MiB; growth: 0.76 MiB
```

RetroArch loaded the core, accepted no-content and XRGB8888 negotiation, and
reported the expected API version, 160x144 geometry, 60 Hz frame rate, and 48
kHz audio rate. Automated normal close/restart and core-switch testing remains
outstanding.

### Cross-platform native matrix

[GitHub Actions run 31901687302](https://github.com/seanburkes/Libretro.CoreKit/actions/runs/31901687302)
enforced a 16 MiB RSS-growth ceiling over 1,000 load/unload cycles and 2,000
managed sessions on every target:

| Target | RSS after first session | Final RSS | Growth |
| --- | ---: | ---: | ---: |
| Linux x64 | 3.66 MiB | 4.39 MiB | 0.73 MiB |
| Linux Arm64 | 2.70 MiB | 3.37 MiB | 0.67 MiB |
| Windows x64 | 7.39 MiB | 8.12 MiB | 0.73 MiB |
| Windows Arm64 | 8.73 MiB | 9.43 MiB | 0.70 MiB |
| macOS x64 | 3.69 MiB | 4.44 MiB | 0.75 MiB |
| macOS Arm64 | 6.86 MiB | 7.67 MiB | 0.81 MiB |

Windows and macOS required no additional keep-resident change for this probe.
That is measured compatibility with the recorded toolchains, not a claim that
NativeAOT library unloading is supported.

## Interpretation

The probe separates two concerns cleanly:

1. The C# ABI, callbacks, frame loop, and explicit managed teardown work.
2. Physically unloading and recreating the NativeAOT runtime does not work
   within a bounded-memory libretro lifecycle on this Linux environment.

The Linux workaround changes `dlclose` into logical teardown: managed session
resources and callback pointers are cleared, but the code and NativeAOT runtime
stay mapped. This matches the Phase 0 fallback criterion, provided later
RetroArch switching tests confirm that resident memory remains bounded.

## Next gate

Stage 0B should stay focused on lifecycle compatibility:

1. Run automated RetroArch close, reload, and conventional-core switch loops on
   Linux and measure process RSS.
2. Repeat the RetroArch lifecycle on Windows x64 and macOS Arm64/x64.
3. Confirm normal frontend exit and process cleanup on each platform.
4. Record a final Phase 0 go/no-go decision before creating `Libretro.Core`.

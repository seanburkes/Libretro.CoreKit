# Phase 0 Compatibility Results

## Current decision

**Linux x64 automated RetroArch gate: provisional go. Overall Phase 0: incomplete.**

The NativeAOT core works through the libretro ABI and an installed RetroArch,
but ordinary NativeAOT unload/reload behavior is not acceptable. Linux is only
provisionally viable when the shared library is marked `NODELETE`, keeping one
runtime resident until the frontend exits.

The native host passes 1,000 loader cycles under a 16 MiB RSS-growth ceiling on
Linux, Windows, and macOS across x64 and Arm64. The Linux x64 RetroArch gate now
also passes 50 managed-to-native core switches, including reset, close, unload,
reload, and normal frontend exit. Do not extract a reusable framework yet: the
equivalent RetroArch tests on Windows and macOS remain outstanding.

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
| RetroArch manual smoke | Flatpak 1.22.2, Git 69a4f0ea1e |
| RetroArch lifecycle gate | source build 1.22.2, Git 7bc72e87359f948f856701cd744dfc2ef8efebaa |
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
kHz audio rate. The automated gate below covers normal close/restart and core
switching.

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

### Linux x64 RetroArch lifecycle

`./eng/run-retroarch-phase-0.sh` built the pinned RetroArch revision, launched
it with an isolated profile, and switched between the NativeAOT probe and an
ABI-equivalent conventional C core 50 times. Every cycle ran and atomically
unloaded the managed core, then ran and atomically unloaded the control core.
Content closure and restart are checked once as a separate transition so the
test does not overlap RetroArch's pending close/reload state. RetroArch exited
normally through its `QUIT` command.

Before the switch loop, the gate attempts to load a missing content path and
confirms that RetroArch returns to contentless state. During the first managed
session it issues save-state and load-state commands, confirms RetroArch reports
that the core does not support save states, and then completes normal teardown.

The pinned source revision is newer than the installed stable build because the
stable build predates the lifecycle command interface used by this automation.
AddressSanitizer found that this revision's UDP poll loop continued using its
command object after `LOAD_CONTENT` or `START_CORE` synchronously rebuilt the
input subsystem and freed that object. The test build applies a pinned one-line
compatibility patch that returns after processing one datagram. A 15-cycle
managed/control run then completed under AddressSanitizer without a memory
error. This changes only the experimental command transport, not libretro core
loading or teardown. The result remains provisional until the fix is available
in a stable frontend baseline or the workflow moves to equivalent stable UI
automation.

```text
managed core mapped after unload: True
RSS after warm-up: 14.66 MiB; peak: 17.74 MiB; growth: 3.08 MiB
PASS: RetroArch load/reset/unload/switch/quit lifecycle
```

An exploratory replay that explicitly combined `VIDEO_REINIT`,
`AUDIO_REINIT`, and `DRIVERS_REINIT` with every core load eventually crashed
RetroArch. An ABI-equivalent conventional C core reproduced the failure, while
75 driver reinits on RetroArch's dummy core passed. The explicit reinit replay
is therefore tracked as a frontend/driver stress issue and is not used to judge
NativeAOT. The blocking gate exercises ordinary core workflows instead.

The first Xvfb CI attempt segfaulted after 39 complete switch cycles while the
conventional C control core was active. A later headless run aborted while the
same control core was active. The command-poller use-after-free explains why
the active core at detection was inconsistent: the frontend heap had already
been corrupted by an earlier lifecycle datagram. CI uses RetroArch's null
video, audio, and input drivers to isolate core loading from virtual SDL; the
SDL configuration remains the local video/audio smoke gate.

## Interpretation

The probe separates two concerns cleanly:

1. The C# ABI, callbacks, frame loop, and explicit managed teardown work.
2. Physically unloading and recreating the NativeAOT runtime does not work
   within a bounded-memory libretro lifecycle on this Linux environment.

The Linux workaround changes `dlclose` into logical teardown: managed session
resources and callback pointers are cleared, but the code and NativeAOT runtime
stay mapped. The real RetroArch switch test confirms that this behavior remains
inside the Phase 0 memory bound on Linux x64.

## Next gate

Stage 0B should stay focused on lifecycle compatibility:

1. Repeat the RetroArch lifecycle on Windows x64 and macOS Arm64/x64.
2. Confirm normal frontend exit and process cleanup on each remaining platform.
3. Record a final Phase 0 go/no-go decision before creating `Libretro.Core`.

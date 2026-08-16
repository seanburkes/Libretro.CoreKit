# Phase 0 Compatibility Results

## Decision

**Go for Phase 1 with Linux x64 as the only initial RetroArch-supported target.**

Linux x64 satisfies the Phase 0 native ABI, allocation, lifecycle, stress,
memory, and real-frontend gates. The other five native targets publish and pass
the independent C host, but they are build-compatibility evidence only. They
are not claimed as RetroArch-supported until the same frontend lifecycle runs
there.

| Target | NativeAOT publish | C host, 1,000 cycles | RetroArch lifecycle | Phase 0 classification |
| --- | --- | --- | --- | --- |
| Linux x64 | Pass | Pass | Pass | Supported Phase 1 baseline |
| Linux Arm64 | Pass | Pass | Untested | Native artifact evidence only |
| Windows x64 | Pass | Pass | Untested | Native artifact evidence only |
| Windows Arm64 | Pass | Pass | Untested | Native artifact evidence only |
| macOS x64 | Pass | Pass | Untested | Native artifact evidence only |
| macOS Arm64 | Pass | Pass | Untested | Native artifact evidence only |

This deliberately narrow claim follows the playbook: an untested platform is
not supported merely because it produced a DLL or dylib.

## Probe scope

The probe provides:

- all 25 mandatory `retro_*` exports with explicit Cdecl entry points;
- matching managed and C assertions for every Phase 0 struct size and offset;
- stable process-lifetime UTF-8 metadata available before `retro_init`;
- explicit initialization, load, unload, deinitialization, and reinitialization;
- no-content and XRGB8888 environment negotiation;
- deterministic 160x144 video and 48 kHz stereo audio;
- RetroPad polling and input-dependent output on every frame;
- a steady-state managed-allocation tripwire around `retro_run`;
- safe unsupported serialization, subsystem, cheat, and memory behavior; and
- machine-readable C-host results with output hashes and memory measurements.

Callbacks are cleared by `retro_deinit`. A frontend must register them again
before starting another managed session.

## Test environment

Recorded on 2026-08-15 and 2026-08-16:

| Component | Version |
| --- | --- |
| Local operating system | Fedora Linux 44, x86-64, kernel 7.1.8 |
| .NET SDK | 10.0.110 |
| .NET runtime / NativeAOT toolchain | 10.0.10 |
| Local C compiler | GCC 16.1.1 |
| CMake | 4.3.0 |
| RetroArch manual smoke | Flatpak 1.22.2, Git 69a4f0ea1e |
| RetroArch lifecycle gate | source build 1.22.2, Git 7bc72e87359f948f856701cd744dfc2ef8efebaa |
| `libretro.h` | `libretro-common` commit 879c8d507b0b52e77e27d759239c2b5df1e26dfd |
| Header SHA-256 | 951c20c2e74b4e1cdfac69b702acb499902e8988e86de973d0922e23f50270ca |

The native matrix uses `ubuntu-22.04`, `ubuntu-22.04-arm`, `windows-2025`,
`windows-11-arm`, `macos-15-intel`, and `macos-15` GitHub-hosted runners.

## Independent native-host evidence

The C11 host compiles against the pinned canonical header with warnings as
errors. It independently checks ABI layouts, resolves all exports, drives valid
and invalid lifecycle orders, validates callback arguments and counts, hashes
video and audio, proves reset returns both hashes to their first-frame values,
and fails if `retro_run` allocates managed memory after initialization.

The final local Linux x64 sanitizer run completed 1,000 loader cycles and 2,000
managed sessions:

```text
ABI: pointer=8, bool=1, system_info=32/8, geometry=20/4,
     timing=16/8, av_info=40/8, game_info=32/8
PASS: 1000 load/unload cycles, 2000 managed sessions
RSS after first session: 11.50 MiB; final: 13.29 MiB; growth: 1.79 MiB
video_first_hash: 83595f10c7712625
audio_first_hash: d9cbd2c2bc6ada79
```

AddressSanitizer and UndefinedBehaviorSanitizer reported no native-host error.
CI runs the same 1,000-cycle assertion on all six native targets and enables the
sanitizers on Linux x64. Each job uploads the core, native host, and JSON result.

The first complete cross-platform run was
[GitHub Actions 31901687302](https://github.com/seanburkes/Libretro.CoreKit/actions/runs/31901687302):

| Target | RSS after first session | Final RSS | Growth |
| --- | ---: | ---: | ---: |
| Linux x64 | 3.66 MiB | 4.39 MiB | 0.73 MiB |
| Linux Arm64 | 2.70 MiB | 3.37 MiB | 0.67 MiB |
| Windows x64 | 7.39 MiB | 8.12 MiB | 0.73 MiB |
| Windows Arm64 | 8.73 MiB | 9.43 MiB | 0.70 MiB |
| macOS x64 | 3.69 MiB | 4.44 MiB | 0.75 MiB |
| macOS Arm64 | 6.86 MiB | 7.67 MiB | 0.81 MiB |

An unmodified Linux NativeAOT shared library retained about 351 MiB over 1,000
close/reopen cycles. Linux therefore links the probe with `-z nodelete`.
Logical teardown resets managed state and callbacks, while one NativeAOT
runtime stays mapped until process exit. With `NODELETE`, growth remains below
the 16 MiB gate. This is an accepted architecture constraint, not physical
library unloading disguised with optimistic wording.

Windows and macOS required no keep-resident linker change in the native host.
The loader calls returned successfully, but physical removal was not promoted
to a support claim without frontend evidence.

## Linux x64 RetroArch evidence

[GitHub Actions 31915782906](https://github.com/seanburkes/Libretro.CoreKit/actions/runs/31915782906)
passed all six native jobs and the Linux x64 RetroArch job. The frontend gate:

- loads and resets the NativeAOT probe without content;
- checks recovery from missing content and unsupported save/load state;
- closes and restarts content;
- switches between the probe and a conventional C control core 50 times;
- verifies the managed module remains deliberately mapped;
- stays below 16 MiB retained RSS growth; and
- exits normally through RetroArch's quit path.

```text
managed core mapped after unload: True
RSS after warm-up: 17.47 MiB; peak: 20.53 MiB; growth: 3.06 MiB
PASS: RetroArch load/reset/unload/switch/quit lifecycle
```

A local SDL-driver run also completed the same ordinary load path, while the C
host objectively validates the video, audio, and input data. Human-visible and
human-audible confirmation is useful smoke testing, but it is not pretending to
be a reproducible CI assertion.

### Frontend harness defect and workaround

AddressSanitizer identified a use-after-free in the pinned RetroArch command
poller. `LOAD_CONTENT` and `START_CORE` synchronously rebuild input, freeing the
UDP command object while its poll loop still uses it. That explains the earlier
nondeterministic crashes with either the managed probe or the conventional C
control core.

The repository applies
`eng/retroarch/0001-netcmd-return-after-reinit.patch`, a one-line test-harness
fix that returns after processing a lifecycle datagram. It changes the
experimental command transport, not the libretro ABI or core teardown. Fifteen
managed/control cycles then passed under AddressSanitizer, and the production
50-cycle gate passed in CI. This patch remains harness debt until an equivalent
stable frontend automation path is available.

## Accepted limitations

- NativeAOT shared-library unloading is unsupported; Linux intentionally uses
  logical teardown plus a process-lifetime resident runtime.
- The initial product claim is Linux x64 only.
- Windows, macOS, and Arm64 require equivalent RetroArch gates before support.
- The pinned RetroArch automation build carries the narrow command-poller fix.
- This is a contentless probe, not yet an emulator adapter or reusable API.

None of these limits blocks a Linux x64-first Phase 1. Expanding the platform
claim without running the frontend there would block it, mostly because wishful
thinking is not a compatibility test.

## Phase 1 promotion set

Promote only the proven pieces:

1. ABI constants, blittable structs, and unmanaged callback signatures.
2. Thin exception-contained exports and the explicit lifecycle owner.
3. Preallocated synchronous video/audio buffers and direct function pointers.
4. The independent native host, output hashes, allocation guard, and CI gates.

Keep the probe rendering/tone behavior as a regression fixture. Do not promote
the RetroArch command patch into product code.

Decision date: 2026-08-16. Repository-owner acceptance is represented by the
requested squash-merge of the Phase 0 decision pull request.

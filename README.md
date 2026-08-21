# Libretro.CoreKit

Libretro.CoreKit aims to be a reusable, strongly typed C# toolkit for building
native [libretro](https://www.libretro.com/) cores that work well with
RetroArch.

The intended foundation is .NET NativeAOT, with explicit native exports, safe
wrappers for video, audio, input, environment, and logging callbacks, and
cross-platform publishing for Windows, Linux, and macOS. The project will also
provide ABI tests against a small native host and progressively validate the
design with a software sample, CHIP-8, and the Craterboy Game Boy emulator.

Phase 0 is complete with a go decision for a Linux x64-first Phase 1. The native
probe passes 1,000 loader cycles on Linux, Windows, and macOS across x64 and
Arm64, but only Linux x64 has passed the real RetroArch lifecycle gate and is
therefore claimed as the initial supported target. Linux deliberately marks the
ELF library `NODELETE` because ordinary NativeAOT unloading retained runtime
state on every reload.

Phase 1 is complete. The reusable `Libretro.Core` assembly now contains the
64-bit ABI layouts, frontend callback signatures, and the selected typed
environment surface for input, core options v2, directories, messages,
language, frame-state hints, no-content support, and pixel-format negotiation.
Native exports intentionally remain in each concrete NativeAOT publishing
project.

Phase 2 is complete. The reusable core contract now covers strict
initialization/content/frame/teardown lifecycle, process-lifetime system
metadata, controller-port device changes, typed RetroPad/video/audio frame
services, exact caller-buffer serialized state, pinned memory regions, and an
audited native bridge for the variadic logging callback. The NativeAOT probe
consumes that host rather than carrying a second lifecycle implementation.

Phase 3 is complete. The contentless software sample renders deterministic
XRGB8888 output, generates stereo audio, responds to RetroPad input, and applies
live tone and palette options without steady-state frame allocations. The C
oracle proves runtime output changes, while the RetroArch gate proves persisted
non-default settings through real frontend lifecycles.

Phase 4 is complete. A separate NativeAOT CHIP-8 core performs bounded
in-memory `.ch8` content loading, standard instruction execution under a
configurable baseline, 60 Hz timers, deterministic random behavior, 64x32
XRGB8888 rendering, complete CHIP-8 keypad mapping across the standard
RetroPad, deterministic sound-timer audio, reset, pinned system RAM, and
transactional state loading. Six independent interpreter and display quirk
options apply at content load and through frontend runtime updates, and
effective choices are preserved in version-4 state.
Malformed execution halts deterministically without partial memory writes, and
the independent host proves byte-identical reset and state replay across stack,
control-flow, opcode, and memory-boundary failures.

See [PLAN.md](PLAN.md) for the overall roadmap and [PHASE-0.md](PHASE-0.md) for
the compatibility-gate playbook. [PHASE-1.md](PHASE-1.md) records the completed
ABI layer, [PHASE-2.md](PHASE-2.md) records the completed reusable host, and
[PHASE-3.md](PHASE-3.md) records the completed software sample.
[PHASE-4.md](PHASE-4.md) tracks the CHIP-8 reference core as it is implemented.
Measurements, limitations, and the Phase 0 decision are in
[docs/phase-0-results.md](docs/phase-0-results.md).

## Run the Linux Stage 0A probe

Requirements are the .NET 10 SDK with NativeAOT prerequisites, a C11 compiler,
CMake, and `nm`:

```sh
./eng/run-phase-0a.sh
```

The script publishes `corekit_probe_libretro.so`, compiles the independent C
host against the pinned canonical `libretro.h`, resolves every mandatory
export, and runs repeated load/session/unload lifecycles. Increase the loader
stress count with `COREKIT_STRESS_CYCLES=1000`.

The generated core is intentionally a probe, not the reusable framework. It
renders a 160x144 XRGB8888 test pattern, submits 48 kHz stereo audio, polls a
RetroPad, supports no-content loading, round-trips deterministic state, and
exposes a writable 64-byte save-memory region. It also advertises one RetroPad
controller configuration so the frontend exercises device-change forwarding.
Its tone and color/monochrome palette options take effect during a loaded
session without restarting the core.

## Run the Linux CHIP-8 gate

```sh
./eng/run-chip8.sh
```

This publishes `corekit_chip8_libretro.so` and runs its focused independent C
host under ASan/UBSan. The host loads deterministic in-memory `.ch8` test
content, verifies arithmetic, memory, font, timer, key-wait, random, drawing,
audible/silent stereo batches, all 16 RetroPad-to-keypad mappings, and
controller disable behavior. It round-trips the versioned state including
deterministic audio phase and interpreter configuration while checking stable
CHIP-8 system RAM. It also exercises the default and alternative behavior of
all six core options. Use
`COREKIT_CHIP8_CYCLES=1000` for the full loader stress count.

## Run the Linux RetroArch lifecycle gate

Requirements additionally include SDL2 development files and X11 (or Xvfb):

```sh
./eng/run-retroarch-phase-0.sh
```

The script builds the pinned RetroArch revision when needed, uses an isolated
profile, switches between the NativeAOT probe and a conventional C control core
50 times, enforces the 16 MiB RSS-growth ceiling, and verifies normal frontend
exit through RetroArch's `QUIT` command. It also verifies recovery from missing
content, content closure, save-memory persistence, and save/load state across
separate frontend process lifecycles. Static metadata ownership and controller
registration/device forwarding are checked in the same frontend gate, along
with persisted non-default core options. It also loads CHIP-8 test content with
all compatibility alternatives persisted, verifies the changed shift behavior,
starts the sound timer, maps all 16 keypad controls, and round-trips a framed
version-4 state plus a halted `0xFFF` boundary state before reset and unload.
The pinned source
build applies the narrow command-poller lifetime fix in
`eng/retroarch/0001-netcmd-return-after-reinit.patch`; without it, lifecycle
commands can free the UDP command object while it is still being polled. Set
`RETROARCH_BINARY` only to reuse a compatible build that includes this fix. CI
runs the SDL2 video/audio path under Xvfb so the timed frontend loop remains
fully unattended. The managed/control switch loop uses
RetroArch's `noload-nosave` mode because the pinned command harness wedges its
dummy-core transition when either conventional C or NativeAOT cores advertise
save RAM; normal persistence is validated in dedicated save/load/quit
lifecycles instead.

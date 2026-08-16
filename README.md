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

Phase 1 is now extracting the proven ABI into a reusable `Libretro.Core`
assembly. The first slice contains the mandatory system/game information
layouts and typed wrappers for no-content and XRGB8888 negotiation; native
exports intentionally remain in each concrete NativeAOT publishing project.

See [PLAN.md](PLAN.md) for the overall roadmap and [PHASE-0.md](PHASE-0.md) for
the compatibility-gate playbook. [PHASE-1.md](PHASE-1.md) tracks the active ABI
work. Measurements, limitations, and the Phase 0 decision are in
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
RetroPad, supports no-content loading, and safely stubs unsupported features.

## Run the Linux RetroArch lifecycle gate

Requirements additionally include SDL2 development files and X11 (or Xvfb):

```sh
./eng/run-retroarch-phase-0.sh
```

The script builds the pinned RetroArch revision when needed, uses an isolated
profile, switches between the NativeAOT probe and a conventional C control core
50 times, enforces the 16 MiB RSS-growth ceiling, and verifies normal frontend
exit through RetroArch's `QUIT` command. It also verifies recovery from missing
content, content closure, and unsupported save/load-state commands. The pinned
source build applies the narrow command-poller lifetime fix in
`eng/retroarch/0001-netcmd-return-after-reinit.patch`; without it, lifecycle
commands can free the UDP command object while it is still being polled. Set
`RETROARCH_BINARY` only to reuse a compatible build that includes this fix. CI
supplies `tests/RetroArch/headless.cfg` to isolate the loader lifecycle from
virtual display and audio drivers; the default local configuration uses SDL2
for the video and audio smoke path.

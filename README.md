# Libretro.CoreKit

Libretro.CoreKit aims to be a reusable, strongly typed C# toolkit for building
native [libretro](https://www.libretro.com/) cores that work well with
RetroArch.

The intended foundation is .NET NativeAOT, with explicit native exports, safe
wrappers for video, audio, input, environment, and logging callbacks, and
cross-platform publishing for Windows, Linux, and macOS. The project will also
provide ABI tests against a small native host and progressively validate the
design with a software sample, CHIP-8, and the Craterboy Game Boy emulator.

The project is currently in Phase 0: proving that a NativeAOT shared library can
survive the complete RetroArch load, run, unload, reload, core-switch, and exit
lifecycle reliably. The Stage 0A native probe now passes 1,000 loader cycles on
Linux, Windows, and macOS across x64 and Arm64. Linux deliberately marks the
ELF library `NODELETE` because ordinary NativeAOT unloading leaked runtime state
on every reload. No production compatibility claim will be made until the
equivalent RetroArch lifecycle gates pass on every supported platform.

See [PLAN.md](PLAN.md) for the overall roadmap and [PHASE-0.md](PHASE-0.md) for
the detailed compatibility-gate playbook. Current measurements and the
provisional decision are in [docs/phase-0-results.md](docs/phase-0-results.md).

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
exit through RetroArch's `QUIT` command. Set `RETROARCH_BINARY` to reuse an
already-built compatible revision. CI supplies `tests/RetroArch/headless.cfg`
to isolate the loader lifecycle from virtual display and audio drivers; the
default local configuration uses SDL2 for the video and audio smoke path.

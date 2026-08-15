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
lifecycle reliably. The Stage 0A Linux probe is implemented; it deliberately
marks the ELF library `NODELETE` because unloading a NativeAOT library leaked
runtime state on every reload. No production compatibility claim will be made
until equivalent lifecycle gates pass on every supported platform.

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

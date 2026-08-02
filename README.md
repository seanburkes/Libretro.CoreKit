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
lifecycle reliably. No production compatibility claim will be made until that
gate passes on the supported platforms.

See [PLAN.md](PLAN.md) for the overall roadmap and [PHASE-0.md](PHASE-0.md) for
the detailed compatibility-gate playbook.

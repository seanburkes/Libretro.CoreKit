# Repository Guidance

## Technical framing

This repository builds .NET NativeAOT libretro cores and validates them in
RetroArch. Describe work with emulator and frontend lifecycle terminology:

- core artifact, native ABI entry point, and frontend callback;
- content load, core initialization, frame execution, reset, teardown, and
  core switching;
- loader close/reopen, logical teardown, and process-lifetime module residency;
- video frame, audio batch, RetroPad state, save data, and serialized state;
- independent C host, RetroArch integration gate, and sanitizer diagnostic.

Prefer precise terms over security-overloaded shorthand. For example, say
"test content" or "callback data" instead of "payload", and say "RetroArch
lifecycle command datagram" instead of "command payload". Document a
memory-safety failure in the pinned frontend test harness as a scoped
lifecycle-harness defect.

Do not hide real defects or alter canonical API names to appease a classifier.
State the context plainly: local emulator compatibility testing using standard
libretro callbacks and deterministic test content. When useful, reproduce a
frontend failure with the conventional C control core to separate frontend
behavior from the managed core.

## Current compatibility decisions

- Linux x64 remains the only RetroArch-supported target for the current phase.
- Other matrix targets prove NativeAOT artifact and C-host compatibility only.
- Linux uses `NODELETE`; `retro_deinit` performs logical teardown while one
  NativeAOT runtime remains mapped until process exit.
- Native exports remain in each concrete publishing assembly because NativeAOT
  does not export `UnmanagedCallersOnly` methods from referenced assemblies.

## Working style

- Build small vertical slices through managed code, the native host, and
  RetroArch when frontend behavior changes.
- Keep the independent C host as the ABI oracle.
- Add only ABI declarations and environment commands exercised by the current
  slice.
- Keep frame execution allocation-free and contain every managed exception at
  the native boundary.
- Record unsupported platforms and deferred interfaces explicitly; a produced
  binary is not proof of frontend support.

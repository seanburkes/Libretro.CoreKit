# Phase 5: Craterboy Readiness Work

**Status:** In progress. The first adapter-facing emulator contract is complete
in `Craterboy.Core`; no Craterboy libretro publishing assembly exists yet.

Phase 5 is intentionally split across repositories. Emulator behavior and
host-neutral buffers belong in
[`craterboy-net`](https://github.com/seanburkes/craterboy-net), while native ABI
entry points, frontend callbacks, and RetroArch lifecycle behavior remain in
Libretro.CoreKit and the future `Craterboy.Libretro` publishing project.

Linux x64 remains the only RetroArch-supported target. Changes to the managed
emulator alone do not expand that compatibility claim.

## Slice 1: Interleaved stereo audio

Completed by
[`craterboy-net` PR #329](https://github.com/seanburkes/craterboy-net/pull/329)
at commit `f2447ba`.

The Craterboy APU now:

- writes complete left/right frames into a preallocated 4,096-frame ring;
- applies NR51 channel routing and NR50 volume independently to each side;
- exposes `Emulator.CopyAudioFrames(Span<short>)`, using libretro-compatible
  left/right interleaving and returning a stereo-frame count;
- rejects odd-length destinations rather than returning a partial frame;
- drops only complete oldest frames when the bounded ring is full; and
- includes both channels and frame-ring indices in deterministic state hashes.

The focused tests distinguish left-only, right-only, asymmetrical-volume,
muted, and identical-default output. A warm steady-state emission and drain
performs zero managed allocations.

Validation on the merged Craterboy revision:

- 581 Release tests pass, including the pinned SameBoy differential suite.
- Focused formatting verification passes for the changed APU and kernel-test
  files.
- The shipping core remains dependency-free and contains no libretro concepts.

## Remaining readiness gate

Before Phase 6 creates `Craterboy.Libretro`, Craterboy still needs:

- a documented playable DMG milestone with its differential and ROM-test gates;
- a stable, allocation-free raw-frame access contract independent of
  presentation palettes;
- explicit versioned state serialization with transactional loading;
- stable save RAM plus defined system RAM, video RAM, and RTC exposure;
- deterministic frame, reset, input, time, and entropy behavior at the public
  adapter boundary; and
- a NativeAOT benchmark with video and audio enabled.

The next narrow slice should formalize and allocation-test the raw 160x144
frame contract. Creating the native adapter before these contracts settle would
only move unfinished emulator ownership into the wrong repository.

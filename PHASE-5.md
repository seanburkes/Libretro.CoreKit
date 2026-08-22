# Phase 5: Craterboy Readiness Work

**Status:** In progress. The stereo-audio and raw-frame adapter contracts are
complete in `Craterboy.Core`; no Craterboy libretro publishing assembly exists
yet.

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

## Slice 2: Stable allocation-free raw frame

Completed by
[`craterboy-net` PR #331](https://github.com/seanburkes/craterboy-net/pull/331)
at commit `8f7eea5`.

The Craterboy PPU now:

- exposes fixed `FrameWidth`, `FrameHeight`, and `FramePixelCount` constants;
- exposes a retained `ReadOnlySpan<ushort>` through `Emulator.RawFrame` without
  copying or permitting callers to mutate the backing buffer;
- identifies the model-native data through `GameBoyFrameFormat`, using hardware
  shade values for monochrome models and RGB15 palette words for color models;
- keeps presentation palette conversion outside the emulator; and
- reuses sprite-composition scratch buffers instead of allocating them during
  frame execution.

The focused tests cover monochrome and color formats, in-place frame updates,
reset clearing through a retained span, and zero managed allocations for a
warmed sprite-enabled frame plus raw-frame access.

Validation on the merged Craterboy revision:

- 587 Release tests pass, including the pinned SameBoy differential suite.
- Focused formatting verification passes for the changed abstraction, PPU, and
  kernel-test files.
- The raw frame remains model-native; the future adapter owns conversion to the
  frontend's negotiated software pixel format.

## Remaining readiness gate

Before Phase 6 creates `Craterboy.Libretro`, Craterboy still needs:

- a documented playable DMG milestone with its differential and ROM-test gates;
- explicit versioned state serialization with transactional loading;
- stable save RAM plus defined system RAM, video RAM, and RTC exposure;
- deterministic frame, reset, input, time, and entropy behavior at the public
  adapter boundary; and
- a NativeAOT benchmark with video and audio enabled.

The next narrow slice should define explicit versioned state serialization and
transactional loading. Creating the native adapter before that contract settles
would only move unfinished emulator ownership into the wrong repository.

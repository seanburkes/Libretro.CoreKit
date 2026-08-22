# Phase 5: Deferred Craterboy Integration

**Status:** Deferred. `Craterboy.Core` is not yet a complete or playable
emulator. The stereo-audio and raw-frame work recorded below remains useful to
Craterboy, but CoreKit no longer schedules further Craterboy work.

Emulator behavior and host-neutral buffers belong in
[`craterboy-net`](https://github.com/seanburkes/craterboy-net). Any future
`Craterboy.Libretro` publishing project will also be owned there and may consume
Libretro.CoreKit after a new integration decision. Native ABI entry points,
frontend callbacks, and RetroArch lifecycle behavior must not enter
`Craterboy.Core`.

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

## Re-entry guard

Do not resume Craterboy integration work from this repository until:

- Craterboy reaches its own documented playable milestone under its own port
  plan and correctness gates;
- the Craterboy repository proposes an adapter based on demonstrated emulator
  requirements rather than anticipated ones; and
- a fresh explicit go decision authorizes the publishing and RetroArch
  integration work.

Until then, do not scaffold `Craterboy.Libretro`, use this document to prioritize
Craterboy work, or add CoreKit abstractions solely for that future adapter. The
two completed slices remain here as historical evidence, not as the beginning
of an active integration sequence.

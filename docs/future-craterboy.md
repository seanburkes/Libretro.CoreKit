# Future Craterboy Integration

Craterboy integration is deferred until that emulator reaches its own
documented playable milestone and receives a fresh explicit go decision. The
Craterboy repository owns its emulator roadmap and any future NativeAOT
publishing project; Libretro.CoreKit does not schedule that work.

Two completed host-neutral Craterboy improvements remain useful independently:

- [`craterboy-net` PR #329](https://github.com/seanburkes/craterboy-net/pull/329)
  added preallocated interleaved stereo output with hardware NR50/NR51 routing.
- [`craterboy-net` PR #331](https://github.com/seanburkes/craterboy-net/pull/331)
  added stable allocation-free 160x144 model-native raw-frame access.

Neither change starts an active adapter sequence. Do not create
`Craterboy.Libretro`, add CoreKit abstractions for anticipated Craterboy needs,
or use this repository to prioritize Craterboy work.

Re-entry requires all of the following:

- Craterboy reaches its own playable milestone under its own correctness gates.
- The Craterboy repository proposes an adapter based on demonstrated emulator
  requirements.
- A fresh explicit go decision authorizes publishing and RetroArch integration.

Any future adapter belongs in the Craterboy repository as a separate publishing
project. `Craterboy.Core` remains managed-only and frontend-independent.

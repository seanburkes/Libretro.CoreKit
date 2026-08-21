# Phase 4: CHIP-8 Reference Core

**Status:** In progress. Two vertical slices now cover bounded `.ch8` content,
the standard instruction set under a documented fixed interpreter baseline,
60 Hz timers, deterministic random behavior, RetroPad input, video, timed
silent audio, reset, and serialized state.

## Implemented slices

The `corekit_chip8_libretro.so` NativeAOT artifact is a separate concrete core,
not another mode in the software probe. It uses the reusable host for the
frontend lifecycle and keeps its required `retro_*` exports in the publishing
assembly.

Implemented behavior:

- Accept in-memory content from 2 through 3,584 bytes and copy it to CHIP-8
  memory at `0x200`; contentless, truncated, and oversized loads are rejected
  before live state changes.
- Execute 12 instructions per 60 Hz frame for a deterministic 720 Hz baseline.
- Implement the standard CHIP-8 instruction set: `00E0`, `00EE`, `1nnn`,
  `2nnn`, `3xnn`, `4xnn`, `5xy0`, `6xnn`, `7xnn`, `8xy0` through `8xy7`,
  `8xyE`, `9xy0`, `Annn`, `Bnnn`, `Cxnn`, `Dxyn`, `Ex9E`, `ExA1`, `Fx07`,
  `Fx0A`, `Fx15`, `Fx18`, `Fx1E`, `Fx29`, `Fx33`, `Fx55`, and `Fx65`.
- Install the conventional 4x5 hexadecimal font at `0x50`, tick delay and
  sound timers once per 60 Hz frame, and continue ticking while `Fx0A` waits.
- Reset deterministic pseudo-random instruction behavior to the fixed
  xorshift32 seed `0xC0DEF00D` on every content load and reset.
- Use a fixed modern interpreter baseline until core options land: shifts use
  `Vx`, logic operations preserve `VF`, `Fx55`/`Fx65` leave `I` unchanged,
  `Bnnn` adds `V0`, `Fx1E` leaves `VF` unchanged, and sprites wrap at display
  edges.
- Halt the virtual machine deterministically on an unsupported or invalid
  instruction without throwing through the native ABI boundary.
- Render the 64x32 monochrome display directly as XRGB8888.
- Submit one preallocated 800-frame silent stereo batch per video frame at
  48 kHz. Audible sound-timer output is deferred, but frontend audio timing is
  not fictional in the meantime.
- Map RetroPad directions to CHIP-8 keys `2`, `8`, `4`, and `6`; A maps to `5`
  and B maps to `0`.
- Expose the pinned 4 KiB CHIP-8 address space as system RAM.
- Serialize registers, stack, display, memory, program counter, index register,
  timers, random state, stack pointer, and halt state in a validated 6,212-byte
  version-2 format. Version-1 states are deliberately rejected rather than
  guessed into a different machine state.
- Preserve the originally loaded content separately so reset restores program
  memory even after execution or frontend memory inspection changes it.

## Validation

The focused independent C host resolves all 25 mandatory exports and verifies:

- rejected call order plus contentless, truncated, and oversized content;
- content copying, XRGB8888 geometry, silent audio timing, and RetroPad polling;
- input-sensitive program output and controller disable behavior;
- arithmetic, skip, transfer, BCD, font, indexed-jump, timer, key-wait, and
  deterministic random instruction behavior through system-RAM observations;
- exact state size, transactional malformed-state rejection, deterministic
  timer/random replay, reset/state restoration, and stable system-RAM pointers;
- repeated logical teardown and loader close/reopen under ASan/UBSan.

The pinned RetroArch gate additionally loads deterministic `.ch8` test content,
runs frames, resets, unloads the core, and observes the managed content-load
lifecycle marker. Linux x64 remains the only RetroArch-supported target.

Current evidence with .NET SDK 10.0.110:

- 1,000 loader close/reopen and logical content lifecycle cycles complete under
  ASan/UBSan with leak detection disabled for the process-lifetime NativeAOT
  runtime and no sanitizer diagnostic.
- Pinned RetroArch `7bc72e8735` accepts the CHIP-8 content lifecycle before the
  existing 50 managed/control switches, state/save process reopens, and normal
  frontend exits. Peak RSS growth is 6.18 MiB under the 16 MiB ceiling.

## Deferred work

Phase 4 is not complete yet. The next slices must add:

- sound-timer-driven audio instead of silence;
- complete keypad mapping and core options for interpreter quirks and display
  behavior;
- broader malformed-program and deterministic replay fixtures;
- a state-format update for configurable quirk behavior.

Generating the repeated native export façade is still deferred until the
Craterboy adapter provides a third concrete consumer. Two copies are mildly
annoying; a generator designed from one and a half examples would be worse.

# S6 — Recording signal combiner (`recording_signal_combiner`)

## Physical process

On a disc with sound, the audio subcarriers (and on later discs the EFM
signal) are added to the video carrier by **symmetrical double-edge
pulse-width modulation**: they move the carrier's zero crossings, which
changes the pit-to-land length ratio (duty cycle) without changing the pit
spacing.

## Sources

- IEC 60857-1986 §7.2.1: audio subcarriers are "symmetrical double-edge
  pulsewidth modulated on the main carrier", citing Panter, *Modulation,
  Noise and Spectral Analysis*, p. 542.
- §7.2.2: subcarrier level −26 ± 1 dB relative to the unmodulated main
  carrier.
- Amendment 2 §13.1.5: EFM, symmetrical double-edge PWM, −27 ± 1 dB.
- BBC RD 1986/01 §1.2: "the composite video signal directly modulates the
  spacing of the pits, while the sound channel modulates the duty cycle".
- Philips Technical Review 33 (1973) pp. 181–182: the VLP prototype carried
  colour and sound as "symmetrical displacements of the zeros" of the limited
  carrier ([secondary sources](secondary-sources.md)).

## Model and equations

The combiner produces the **slice level** *s*(t) that S7 compares the carrier
against:

- a pit edge is placed where `cos φ(t) = s(t)` (S7);
- with *s* = 0 the crossings are at φ = (k + ½)π, giving a 50 % duty cycle;
- a slowly varying *s* moves the rising and falling crossings of each cycle
  in opposite directions by equal amounts, which is symmetrical double-edge
  PWM.

The mapping from audio and EFM signals to *s*(t), and its calibration to the
§7.2.2 and §13.1.5 levels, is later work.

## Parameters

None in the video-only scope.

## Assumptions and approximations

None in the video-only scope: *s* ≡ 0 exactly.

## Input/output contract

- **In/out:** D3. In the video-only scope, *s* ≡ 0 and φ passes through
  unchanged.

## Project YAML block

None in the video-only scope. The block name `recording_signal_combiner` is
reserved; any key in it is a validation error until audio or EFM is
implemented.

## Ideal setting

Pass-through (video only).

## Verification

Identity unit test (`tests/unit/test_recording_signal_combiner.cpp`): D3
out equals D3 in.

## Diagnostic tap

None: the stage is an identity in the video-only scope.

## Open items

- Panter p. 542 has not been read (loan-only copy). Its subject, the
  spectrum of symmetrical double-edge PWM, is available from Sun (2012),
  eqs (2.15) and (2.17)
  ([secondary sources](secondary-sources.md#open-access-sources-from-the-wider-search)),
  for a triangle carrier. LaserVision limits a sinusoidal carrier, so the
  audio work must derive the corresponding spectrum. Needed for audio, not
  for video.

# Changes

A running log of the recent visualizer work, newest first. Tuning constants
referenced below all live at the top of [`src/main.cpp`](src/main.cpp).

## Unreleased (working tree)

**Blended frequency axis + spectral tilt.**

- Replaced the pure log-spaced buckets with a **linear/log blend**. Pixel
  frequencies interpolate between the two mappings via a single `BLEND` knob
  (`0` = linear, `1` = log; default `0.4`). At the ~52Hz bin width a pure log
  axis collapses many low pixels onto the same FFT bin, giving blocky bass;
  linear spreads them out. The shared `pixel_freq()` helper drives both the
  bucket edges and the tilt weights so they stay consistent.
- Added a **spectral tilt weight**: each bucket's power is scaled by
  `(f/FREQ_MIN)^TILT` (slope ≈ `3*TILT` dB/octave) to offset the bass-heavy
  energy of typical music that the flatter axis would otherwise let dominate.
  `TILT` defaults to `0.5` (~+1.5dB/oct); `1.0` is a full pink correction but
  makes the high end oversensitive.
- Updated the README Signal path and Tuning sections for `BLEND`/`TILT`, and
  tidied lint (bare URLs → links, "Nexopixel" typo, trailing blank line).

## 530cda1 — "more tweakage"

Multi-platform build support: targets for `rp2040`, `rp2350-arm-s`, and
`rp2350-riscv`, with matching build instructions in the README, plus
`.gitignore`/`CMakeLists.txt` adjustments.

## 6cb3fb9 — "tweak"

Brightness pipeline rework and tuning docs:

- Extended the displayed band up to 8kHz.
- Replaced the `AGC_FLOOR` silence threshold with a `REF_POWER` reference
  model — a downward-only AGC that is pushed up by loud buckets and decays
  back toward `REF_POWER`, so the display never clips.
- Added `MASTER_BRIGHTNESS` and `GAMMA` (perceptual contrast) to the final
  pixel scaling.
- Added the symptom-first **Tuning** section to the README.

## 6fd8db4 — "i for one welcome my new robot overlords"

Major rewrite of the visualizer pipeline, replacing the naïve fixed-size
linear bucket averaging with:

- **Log-spaced fractional-bin buckets** (`bucket_edges()` + `band_power()`),
  integrating power across partial bins at each edge.
- A **Hann window** applied before the FFT to cut spectral leakage
  (`window` buffer added to `ADCFFT`).
- An **adaptive-peak AGC** with dB-relative brightness mapping and
  fast-attack / slow-decay pixel trails (`PIXEL_DECAY`).
- Static sample/FFT buffers (too large for the core-0 stack).
- The initial **Signal path** section in the README.

# pico-neospec

Microphone -> Pico ADC -> FFT -> Power spectrum -> Neopixel

FFT based on code [here](https://github.com/AlexFWulff/awulff-pico-playground).

Neopixel based on code [here](https://github.com/raspberrypi/pico-examples/tree/master/pio/ws2812).

## Prerequisites

Microphone: I used a DFRobot MEMS microphone, which has a bandwidth of 100-8kHz, connected to ADC 1 (pin 27).

Neopixel: I used a 96 RGB LED connected to pin 0.

(Modify `NPX_PIN` and `CAPTURE_CHANNEL` as necessary)

Main loop (sample, FFT, write to Neopixel) timings (960 samples @ 50kHz) are approx:

Board        | Time (ms)
-------------|-----
RP2040       | 40
RP2350 ARM   | 22
RP2350 RISCV | 31

NB Sampling 960 times at 50kHz takes 921600 cycles or 19.2 ms @ 48MHz.

## Signal path

Each frame: sample -> window -> FFT -> frequency buckets -> tilt weight -> noise gate -> dB brightness -> Neopixel.

1. **Sample**: 960 8-bit samples at 50kHz via DMA (19.2ms). The DC component (mean) is subtracted.
2. **Window**: a Hann window is applied before the FFT to reduce spectral leakage, which would otherwise smear strong (especially bass) tones across neighbouring bins.
3. **FFT**: 960-point real FFT (kiss_fft), giving 481 usable bins of width 50000/960 ≈ 52.1Hz.
4. **Buckets**: the 96 pixels span 60Hz to 8kHz, with pixel frequencies blended between a linear and a log mapping via `BLEND` (0 = linear, 1 = log). A pure log axis is a piano-key layout, but at this bin width (~52Hz) it collapses many low pixels onto the same bin, giving blocky bass; linear spreads them out but crams the low octaves. The default `BLEND` = 0.4 is a compromise. Bucket edges are fractional FFT bin indices and power (|X|²) is integrated over each bucket with proportional contributions from the partial bins at the edges.
5. **Tilt weight**: each bucket's power is multiplied by `(f/FREQ_MIN)^TILT`, using the bucket centre frequency, to offset the bass-heavy energy of typical music that linear-ish spacing would otherwise let dominate. The exponent maps to a slope of ~`3*TILT` dB/octave: `TILT` = 1 is a full pink-noise correction (+3dB/oct), which over the 60Hz–8kHz range (≈7 octaves) is a ~133x power boost at the top — usually too much, making the high end oversensitive. `TILT` = 0.5 (the default, ~+1.5dB/oct, ~12x at 8kHz) is a gentler tilt; 0 disables it entirely and leaves the raw spectrum.
6. **Noise gate**: the background (mic hiss, ADC quantisation, supply ripple) is roughly flat per FFT bin, so the wide, tilt-boosted top buckets would otherwise glow even in silence. Each bucket keeps an adaptive noise floor that follows a smoothed copy of its power down quickly but rises only ~0.1dB/s (`NOISE_RISE`), so it settles on the background without absorbing notes; `NOISE_GATE` times the floor (4x = 6dB) is subtracted from the bucket power.
7. **Brightness**: each bucket's power is expressed in dB relative to a reference. The reference sits at `REF_POWER` (which therefore sets the absolute sensitivity: quiet music renders dim, silence dark) but is pushed up by louder buckets and decays back at 0.5% per frame — downward-only AGC, so the display never clips. The 30dB (`DYNAMIC_RANGE_DB`) below the reference maps linearly to a level 0..1; pixels have fast attack and slow decay (`PIXEL_DECAY`) for the classic analyser trail. Finally the level is gamma-corrected (`GAMMA`) and scaled by `MASTER_BRIGHTNESS` before being written to the strip.

## Tuning

All constants are at the top of `src/main.cpp` unless noted.

Symptom-first:

* *Quiet music saturates the strip* → raise `REF_POWER` (it is the bucket power that gives full brightness — 4x ≈ 6dB less sensitive).
* *Strip barely lights even for loud music* → lower `REF_POWER`.
* *Too bright overall (but relative levels look right)* → lower `MASTER_BRIGHTNESS`.
* *Everything that lights is at similar brightness / washed out* → raise `GAMMA` for more contrast, or reduce `DYNAMIC_RANGE_DB` so only stronger buckets register.
* *Quiet harmonics invisible* → raise `DYNAMIC_RANGE_DB` (each bucket's brightness reflects how far it is below the reference, so a bigger range keeps weaker content visible) or lower `GAMMA` (1.0 = none).
* *Display pumps up and down with volume* → the AGC recovery; raise `AGC_DECAY` towards 1 for slower, steadier gain (0.995 ≈ halves in ~7s at 20fps).
* *Trails too short/jumpy or too smeared* → `PIXEL_DECAY` is the per-frame fade (0.75 ≈ 0.4s to dark); higher = longer trails.
* *Bass notes appear as wide blocks* → lower `BLEND` towards 0 (more linear spacing spreads the low bins over fewer pixels each). Alternatively increase `ADCFFT::SAMPLE_SIZE` (in `src/adcfft.h`, e.g. 2400 for ~21Hz bins) for finer bins — but that costs frame rate, as the FFT is software float on RP2040.
* *Top end lit / twinkling in silence* → raise `NOISE_GATE` (or lower `TILT`); lower `NOISE_GATE` if quiet content disappears. On a Pico/Pico 2 (not W), driving GPIO23 high puts the onboard SMPS in PWM mode, which reduces ADC noise.
* *Bass swamps the display / treble too dim* → raise `TILT` for more high-frequency boost; lower it (towards 0 = flat) if the top end is now overpowering.
* *Low octaves feel too cramped* → raise `BLEND` towards 1 (more log spacing gives the bass more pixels), at the cost of some blockiness.
* *Wrong band of interest* → `FREQ_MIN`/`FREQ_MAX` set the axis ends. Keep `FREQ_MAX` ≤ mic bandwidth and ≤ `SAMPLE_FREQ/2`; lowering `FREQ_MIN` below the FFT bin width (~52Hz) just stretches the lowest bin over more pixels.

A host-side simulation of the exact pipeline (using the repo's kiss_fft on synthetic tones) is a quick way to preview parameter changes without flashing.

## Build

Available targets are: rp2040, rp2350-arm-s, rp2350-riscv

```sh
mkdir build_pico2_arm
cd build_pico2_arm
cmake -DPICO_PLATFORM=rp2350-arm-s ..
make -j
```

Builds main binary plus individual test harnesses for ADCFFT and Neopixel

Copy the uf2 file to device, powered with reset button pressed.

## Inspect

```sh
minicom -b 115200 -D /dev/ttyACM0
```

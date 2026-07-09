# pico-neospec

Microphone -> Pico ADC -> FFT -> Power spectrum -> Neopixel

FFT based on code here: https://github.com/AlexFWulff/awulff-pico-playground

Nexopixel based on code here: https://github.com/raspberrypi/pico-examples/tree/master/pio/ws2812

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

Each frame: sample -> window -> FFT -> log-frequency buckets -> dB brightness -> Neopixel.

1. **Sample**: 960 8-bit samples at 50kHz via DMA (19.2ms). The DC component (mean) is subtracted.
2. **Window**: a Hann window is applied before the FFT to reduce spectral leakage, which would otherwise smear strong (especially bass) tones across neighbouring bins.
3. **FFT**: 960-point real FFT (kiss_fft), giving 481 usable bins of width 50000/960 ≈ 52.1Hz.
4. **Buckets**: the 96 pixels are log-spaced in frequency from 60Hz to 8kHz, i.e. each pixel spans a constant frequency *ratio* (8000/60)^(1/96) ≈ 1.052, about 0.9 semitones — a piano-key axis rather than a linear one, since musical energy is concentrated in the low octaves. Bucket edges are fractional FFT bin indices and power (|X|²) is integrated over each bucket with proportional contributions from the partial bins at the edges: at the red end a pixel is narrower than one bin, at the blue end it spans ~8 bins. Summing (not averaging) over the wider high-frequency buckets adds a +3dB/octave tilt that roughly offsets the pink (−3dB/octave) spectrum of typical music.
5. **Brightness**: each bucket's power is expressed in dB relative to a reference. The reference sits at `REF_POWER` (which therefore sets the absolute sensitivity: quiet music renders dim, silence dark) but is pushed up by louder buckets and decays back at 0.5% per frame — downward-only AGC, so the display never clips. The 30dB (`DYNAMIC_RANGE_DB`) below the reference maps linearly to a level 0..1; pixels have fast attack and slow decay (`PIXEL_DECAY`) for the classic analyser trail. Finally the level is gamma-corrected (`GAMMA`) and scaled by `MASTER_BRIGHTNESS` before being written to the strip.

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
* *Bass notes appear as wide blocks* → increase `ADCFFT::SAMPLE_SIZE` (in `src/adcfft.h`, e.g. 2400 for ~21Hz bins). Costs frame rate: the FFT is software float on RP2040.
* *Wrong band of interest* → `FREQ_MIN`/`FREQ_MAX` set the log axis ends. Keep `FREQ_MAX` ≤ mic bandwidth and ≤ `SAMPLE_FREQ/2`; lowering `FREQ_MIN` below the FFT bin width (~52Hz) just stretches the lowest bin over more pixels.

A host-side simulation of the exact pipeline (using the repo's kiss_fft on synthetic tones) is a quick way to preview parameter changes without flashing.

## Build

```sh
mkdir -p build
cd build
cmake -DPICO_PLATFORM=rp2040 ..
make -j
```

Builds main binary plus individual test harnesses for ADCFFT and Neopixel

Copy the uf2 file to device, powered with reset button pressed.

## Inspect

```sh
minicom -b 115200 -D /dev/ttyACM0
```


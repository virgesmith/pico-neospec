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
4. **Buckets**: the 96 pixels are log-spaced in frequency from 60Hz to 5kHz, i.e. each pixel spans a constant frequency *ratio* (5000/60)^(1/96) ≈ 1.047, about 0.8 semitones — a piano-key axis rather than a linear one, since musical energy is concentrated in the low octaves. Bucket edges are fractional FFT bin indices and power (|X|²) is integrated over each bucket with proportional contributions from the partial bins at the edges: at the red end a pixel is narrower than one bin, at the blue end it spans ~5 bins. Summing (not averaging) over the wider high-frequency buckets adds a +3dB/octave tilt that roughly offsets the pink (−3dB/octave) spectrum of typical music.
5. **Brightness**: each bucket's power is expressed in dB relative to an adaptive peak — the loudest bucket, decaying 0.5% per frame with a floor (`AGC_FLOOR`) that keeps a silent room dark. The 30dB (`DYNAMIC_RANGE_DB`) below the peak maps linearly to brightness, so quiet harmonics still register. Pixels have fast attack and slow decay (`PIXEL_DECAY`, 25% fade per frame) for the classic analyser trail.

Tuning constants live at the top of `src/main.cpp`. For finer bass resolution increase `ADCFFT::SAMPLE_SIZE` (e.g. to 2400 for ~21Hz bins) at the cost of frame rate.

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


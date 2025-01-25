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

At a sampling rate of 50kHz, each sample takes 960x960

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


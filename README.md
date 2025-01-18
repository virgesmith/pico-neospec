# pico-neospec

Pico ADC -> FFT -> Power spectrum -> Neopixel

FFT based on code here: https://github.com/AlexFWulff/awulff-pico-playground

Nexopixel based on code here: https://github.com/raspberrypi/pico-examples/tree/master/pio/ws2812

## Prerequisites

Neopixel (96 LEDS) connected to pin 0.

Input signal connected to ADC 0 (pin 26). Neopixel at max brightness with around 3.3V peak-peak

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


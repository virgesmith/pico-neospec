#include "neopixel.h"
#include "adcfft.h"

#include "pico/stdlib.h"

#include <array>
#include <cmath>
#include <cstdio>

constexpr uint NPX_PIN = 0;
constexpr uint NPX_PIXELS = 96;

// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
constexpr uint CLOCK_DIV = 960;
constexpr uint SAMPLE_FREQ = 50000;
constexpr uint8_t CAPTURE_CHANNEL = 0;

float* pixelise_p(const kiss_fft_cpx* fft_out, float* power) {

  auto BUCKET_SIZE = ADCFFT::SAMPLE_SIZE / NPX_PIXELS;

  float maxp = 2.0e9; // ~3.3V p-p
  for (size_t i = 0; i < NPX_PIXELS; ++i) {
    // any frequency bin over NSAMP/2 is aliased (nyquist sampling theorem)
    auto idx = i * BUCKET_SIZE / 2;
    power[i] = 0.0;
    for (size_t j = idx; j < idx + BUCKET_SIZE / 2; ++j) {
      power[i] += fft_out[j].r * fft_out[j].r + fft_out[j].i * fft_out[j].i;
    }
    //maxp = power[i] > maxp ? power[i] : maxp;
  }
  // normalise
  for (size_t i = 0; i < NPX_PIXELS; ++i) {
    power[i] /= maxp;
  }
  return power;
}

int main() {
  stdio_init_all();

  NeoPixel npx(NPX_PIN, NPX_PIXELS);
  std::array<RGBW, NPX_PIXELS> palette;
  set_palette_spectrum(palette);

  ADCFFT adcfft(CAPTURE_CHANNEL, CLOCK_DIV, SAMPLE_FREQ);

  const float* freqs = adcfft.frequency_bins();

  float power[NPX_PIXELS];

  for (;;) {
    pixelise_p(adcfft.sample_raw(), power);

    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      npx.setpixel(palette[i].scaled(power[i]));
    }
  }
}


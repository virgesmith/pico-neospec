#include "adcfft.h"
#include "neopixel.h"

#include "pico/stdlib.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

constexpr uint NPX_PIN = 0;
constexpr uint NPX_PIXELS = 96;

// Max sample rate is 500kHz @ 48MHz clock (96 cycles per sample)
constexpr uint SAMPLE_FREQ = 50'000;
constexpr uint SAMPLE_SIZE = 960;
constexpr uint8_t CAPTURE_CHANNEL = 1;

// TODO align with the mic bandwidth e.g. 100Hz~8KHz
const std::vector<float>& pixelise_p(const std::vector<kiss_fft_cpx>& fft, std::vector<float>& power) {

  auto BUCKET_SIZE = fft.size() / NPX_PIXELS;

  float maxp = 2.0e9; // ~3.3V p-p
  for (size_t i = 0; i < NPX_PIXELS; ++i) {
    // any frequency bin over sample_size/2 is aliased (nyquist sampling theorem)
    auto idx = i * BUCKET_SIZE / 2;
    power[i] = 0.0;
    for (size_t j = idx; j < idx + BUCKET_SIZE / 2; ++j) {
      power[i] += fft[j].r * fft[j].r + fft[j].i * fft[j].i;
    }
    // maxp = power[i] > maxp ? power[i] : maxp;
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

  ADCFFT adcfft(CAPTURE_CHANNEL, SAMPLE_FREQ, SAMPLE_SIZE);

  auto freqs = adcfft.frequency_bins();

  std::vector<float> power(NPX_PIXELS);

  for (;;) {
    pixelise_p(adcfft.sample_raw(), power);

    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      npx.setpixel(palette[i].scaled(power[i]));
    }
  }
}

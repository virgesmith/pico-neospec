#include "adcfft.h"
#include "neopixel.h"

#include "pico/stdlib.h"

#include <array>
#include <cmath>
#include <cstdio>

constexpr uint NPX_PIN = 0;
constexpr uint NPX_PIXELS = 96;

// Max sample rate is 500kHz @ 48MHz clock (96 cycles per sample)
constexpr uint SAMPLE_FREQ = 50'000;
constexpr uint8_t CAPTURE_CHANNEL = 1;

// display up to 5kHz (mic bw is 100-8kHz)
constexpr uint NPX_BANDWIDTH = 5'000;

const std::array<float, NPX_PIXELS>& pixelise_p(const ADCFFT::array<kiss_fft_cpx>& fft, uint cutoff,
                                                std::array<float, NPX_PIXELS>& power) {

  auto BUCKET_SIZE = std::min(1u, cutoff / NPX_PIXELS);

  float maxp = 2.0e7; // set by experimentation
  for (size_t i = 0; i < NPX_PIXELS; ++i) {
    // any frequency bin over sample_size/2 is aliased (nyquist sampling theorem)
    auto idx = i * BUCKET_SIZE;
    power[i] = 0.0;
    for (size_t j = idx; j < idx + BUCKET_SIZE; ++j) {
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

  ADCFFT adcfft(CAPTURE_CHANNEL, SAMPLE_FREQ);

  const auto& freqs = adcfft.frequency_bins();

  std::array<float, NPX_PIXELS> power;

  const uint cutoff = ADCFFT::SAMPLE_SIZE * (float(NPX_BANDWIDTH) / SAMPLE_FREQ);

  for (;;) {
    // auto t = time_us_64();
    pixelise_p(adcfft.sample_raw(), cutoff, power);

    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      npx.setpixel(palette[i].scaled(power[i]));
    }
    // t = time_us_64() - t;
    // printf("dt=%lldus\n", t);
  }
}

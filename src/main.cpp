#include "adcfft.h"
#include "neopixel.h"

#include "pico/stdlib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

constexpr uint NPX_PIN = 0;
constexpr uint NPX_PIXELS = 96;

// Max sample rate is 500kHz @ 48MHz clock (96 cycles per sample)
constexpr uint SAMPLE_FREQ = 50'000;
constexpr uint8_t CAPTURE_CHANNEL = 1;

// displayed band, log-spaced across the strip (mic bw is 100-8kHz)
constexpr float FREQ_MIN = 60.0f;
constexpr float FREQ_MAX = 8'000.0f;

// bucket power giving full brightness - the main sensitivity control:
// raise if quiet music saturates the strip, lower if it barely registers.
// Louder buckets push the reference up (AGC) so the display never clips.
constexpr float REF_POWER = 2.0e7f;
// brightness spans this much power below the reference
constexpr float DYNAMIC_RANGE_DB = 30.0f;
constexpr float AGC_DECAY = 0.995f;  // reference decay per frame back toward REF_POWER
constexpr float PIXEL_DECAY = 0.75f; // lit pixels fade 25% per frame

constexpr float MASTER_BRIGHTNESS = 0.5f; // overall LED intensity
constexpr float GAMMA = 2.2f;             // perceptual correction: higher = more contrast

// fractional fft bin index of each pixel boundary, log-spaced in frequency
std::array<float, NPX_PIXELS + 1> bucket_edges() {
  std::array<float, NPX_PIXELS + 1> edges;
  const float bin_width = float(SAMPLE_FREQ) / ADCFFT::SAMPLE_SIZE;
  for (size_t i = 0; i <= NPX_PIXELS; ++i) {
    edges[i] = FREQ_MIN * std::pow(FREQ_MAX / FREQ_MIN, float(i) / NPX_PIXELS) / bin_width;
  }
  return edges;
}

// integrate power over the fractional bin range [lo, hi)
float band_power(const ADCFFT::array<kiss_fft_cpx>& fft, float lo, float hi) {
  auto bin = [&fft](size_t j) { return fft[j].r * fft[j].r + fft[j].i * fft[j].i; };
  const auto ilo = size_t(lo);
  const auto ihi = size_t(hi);
  if (ilo == ihi) {
    return bin(ilo) * (hi - lo);
  }
  float p = bin(ilo) * (ilo + 1 - lo) + bin(ihi) * (hi - ihi);
  for (size_t j = ilo + 1; j < ihi; ++j) {
    p += bin(j);
  }
  return p;
}

int main() {
  stdio_init_all();

  NeoPixel npx(NPX_PIN, NPX_PIXELS);
  std::array<RGBW, NPX_PIXELS> palette;
  set_palette_spectrum(palette);

  // static: sample/fft buffers are far too big for the core-0 stack
  static ADCFFT adcfft(CAPTURE_CHANNEL, SAMPLE_FREQ);

  const auto edges = bucket_edges();

  std::array<float, NPX_PIXELS> power;
  std::array<float, NPX_PIXELS> level{};

  float peak = REF_POWER;

  for (;;) {
    const auto& fft = adcfft.sample_raw();

    float frame_max = 0.0f;
    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      power[i] = band_power(fft, edges[i], edges[i + 1]);
      frame_max = std::max(frame_max, power[i]);
    }

    // gain reference tracks the loudest bucket, decaying slowly toward REF_POWER
    peak = std::max({frame_max, peak * AGC_DECAY, REF_POWER});

    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      float db = 10.0f * std::log10(power[i] / peak + 1e-12f);
      float b = std::clamp(1.0f + db / DYNAMIC_RANGE_DB, 0.0f, 1.0f);
      // fast attack, slow decay
      level[i] = std::max(b, level[i] * PIXEL_DECAY);
      npx.setpixel(palette[i].scaled(MASTER_BRIGHTNESS * std::pow(level[i], GAMMA)));
    }
  }
}

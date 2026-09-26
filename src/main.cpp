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

// displayed band (mic bw is 100-8kHz). pixel frequencies blend between a
// linear and a log mapping via BLEND: 0 = fully linear, 1 = fully log.
// pure log gives blocky bass at this bin width (~52Hz/bin) as many pixels
// collapse onto the same low-freq bin; linear spreads them out. a partial
// blend keeps some bass resolution without the blockiness.
constexpr float FREQ_MIN = 60.0f;
constexpr float FREQ_MAX = 8'000.0f;
constexpr float BLEND = 0.4f;

// spectral tilt to counter music's bass-heavy energy under (mostly) linear
// spacing: power is weighted by (f/FREQ_MIN)^TILT, boosting higher bands.
// the slope is ~3*TILT dB/octave, so TILT=1 is a full pink (+3dB/oct)
// correction -- a ~133x boost from 60Hz to 8kHz. lower if the top end is
// too sensitive; 0 disables the tilt entirely.
constexpr float TILT = 0.5f;

// blended pixel frequency for fractional position x in [0, 1]
float pixel_freq(float x) {
  const float lin = FREQ_MIN + (FREQ_MAX - FREQ_MIN) * x;
  const float log = FREQ_MIN * std::pow(FREQ_MAX / FREQ_MIN, x);
  return (1.0f - BLEND) * lin + BLEND * log;
}

// bucket power giving full brightness - the main sensitivity control:
// raise if quiet music saturates the strip, lower if it barely registers.
// Louder buckets push the reference up (AGC) so the display never clips.
constexpr float REF_POWER = 1.0e7f;
// brightness spans this much power below the reference
constexpr float DYNAMIC_RANGE_DB = 30.0f;
constexpr float AGC_DECAY = 0.995f;  // reference decay per frame back toward REF_POWER
constexpr float PIXEL_DECAY = 0.75f; // lit pixels fade 25% per frame

// adaptive per-bucket noise floor: the floor follows a smoothed copy of each
// bucket's power down quickly but creeps up only slowly, so it settles on the
// background (mic hiss, ADC/supply noise) without swallowing notes. NOISE_GATE
// times the floor is subtracted from each bucket before the dB mapping. the
// background is roughly flat per fft bin, so without this the wide, tilted
// top buckets glow even in silence. raise NOISE_GATE if the strip still
// twinkles in silence; lower it if quiet content is lost.
constexpr float NOISE_SMOOTH = 0.1f; // smoothing of bucket power fed to the floor (per frame)
constexpr float NOISE_RISE = 1.001f; // max floor rise per frame (~0.1dB/s at 25fps)
constexpr float NOISE_GATE = 4.0f;   // floor multiple subtracted (4x = 6dB margin)

constexpr float MASTER_BRIGHTNESS = 0.5f; // overall LED intensity
constexpr float GAMMA = 2.2f;             // perceptual correction: higher = more contrast

// fractional fft bin index of each pixel boundary, blended lin/log in frequency
std::array<float, NPX_PIXELS + 1> bucket_edges() {
  std::array<float, NPX_PIXELS + 1> edges;
  const float bin_width = float(SAMPLE_FREQ) / ADCFFT::SAMPLE_SIZE;
  for (size_t i = 0; i <= NPX_PIXELS; ++i) {
    edges[i] = pixel_freq(float(i) / NPX_PIXELS) / bin_width;
  }
  return edges;
}

// per-pixel power weight applying the spectral tilt (see TILT)
std::array<float, NPX_PIXELS> bucket_weights() {
  std::array<float, NPX_PIXELS> weights;
  for (size_t i = 0; i < NPX_PIXELS; ++i) {
    const float freq = pixel_freq((float(i) + 0.5f) / NPX_PIXELS);
    weights[i] = std::pow(freq / FREQ_MIN, TILT);
  }
  return weights;
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
  const auto weights = bucket_weights();

  std::array<float, NPX_PIXELS> power;
  std::array<float, NPX_PIXELS> level{};

  auto measure = [&]() {
    const auto& fft = adcfft.sample_raw();
    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      power[i] = band_power(fft, edges[i], edges[i + 1]) * weights[i];
    }
  };

  // seed the noise floor from a first frame
  measure();
  auto smoothed = power;
  auto noise = power;

  float peak = REF_POWER;

  for (;;) {
    measure();

    float frame_max = 0.0f;
    for (size_t i = 0; i < NPX_PIXELS; ++i) {
      smoothed[i] += NOISE_SMOOTH * (power[i] - smoothed[i]);
      noise[i] = std::min(noise[i] * NOISE_RISE, smoothed[i]);
      power[i] = std::max(power[i] - NOISE_GATE * noise[i], 0.0f);
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

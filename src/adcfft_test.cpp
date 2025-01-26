#include "adcfft.h"

#include "pico/stdlib.h"

#include <cmath>
#include <cstdio>

constexpr uint SAMPLE_FREQ = 50'000;
constexpr uint SAMPLE_SIZE = 960;

// Channel 0 is GPIO26
constexpr uint8_t CAPTURE_CHANNEL = 1;
constexpr uint8_t LED_PIN = 18; // 25 is the (non-W) onboard LED

int main() {
  stdio_init_all();

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  ADCFFT adcfft(CAPTURE_CHANNEL, SAMPLE_FREQ, SAMPLE_SIZE);

  auto freqs = adcfft.frequency_bins();

  auto t = time_us_64();
  auto delay_us = 1000000ull;

  for (;;) {
    gpio_put(LED_PIN, 1);
    // get SAMPLE_SIZE samples at SAMPLE_FREQ
    auto fft_out = adcfft.sample_raw();
    gpio_put(LED_PIN, 0);

    // compute power and calculate max freq component
    float max_power = 0.0;
    int max_idx = 0;
    // any frequency bin over sample_size/2 is aliased (nyquist sampling theorem)
    for (int i = 0; i < adcfft.sample_size / 2; i++) {
      float power = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
      if (power > max_power) {
        max_power = power;
        max_idx = i;
      }
    }

    auto tnew = time_us_64();
    printf("fpeak=%.1fHz dt=%lldus\n", freqs[max_idx], (tnew - t) - delay_us);
    t = tnew;
    sleep_us(delay_us);
  }
}

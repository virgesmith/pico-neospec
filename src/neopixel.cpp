#include "neopixel.h"

#include "ws2812/ws2812.pio.h"

#include <algorithm>
#include <array>
#include <utility>

#include <cstdio>
#include <cstdlib>

RGBW RGBW::scaled(float scale) {
  auto s = [scale](uint8_t c) { return uint8_t(std::clamp(c * scale, 0.0f, 255.0f)); };
  return RGBW(s(rgbw[R]), s(rgbw[G]), s(rgbw[B]), s(rgbw[W]));
}

NeoPixel::NeoPixel(int pin, int num_pixels) : pin(pin), num_pixels(num_pixels), _idx(0) {
  // This will find a free pio and state machine for our program and load it for us
  // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
  // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and supported by the hardware

  // todo get free sm
  bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, pin, 1, true);
  hard_assert(success);

  ws2812_program_init(pio, sm, offset, pin, 800000, /*IS_RGBW=*/false);
}

int NeoPixel::setpixel(uint32_t pixel_grb) {
  pio_sm_put_blocking(pio, sm, pixel_grb);
  _idx = (_idx + 1) % num_pixels;
  return _idx;
}

NeoPixel::~NeoPixel() { pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset); }

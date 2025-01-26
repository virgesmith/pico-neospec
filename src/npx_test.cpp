#include "neopixel.h"

#include "pico/stdlib.h"

#include <array>
#include <utility>
#include <functional>

#include <cstdio>
#include <cstdlib>


constexpr const int NUM_PIXELS = 96;
constexpr int WS2812_PIN = 0;
constexpr bool IS_RGBW = false;

std::array<RGBW, NUM_PIXELS> palette;


void pattern_snakes(NeoPixel& npx, uint t) {
  for (uint i = 0; i < npx.num_pixels; ++i) {
    uint x = (i + (t >> 1)) % 64;
    if (x < 10)
      npx.setpixel(RED);
    else if (x >= 15 && x < 25)
      npx.setpixel(GREEN);
    else if (x >= 30 && x < 40)
      npx.setpixel(BLUE);
    else
      npx.setpixel(0);
  }
}

void pattern_random(NeoPixel& npx, uint t) {
  if (t % 8)
    return;
  for (uint i = 0; i < npx.num_pixels; ++i)
   npx.setpixel(rand() % 2 ? palette[i].u32 : 0u);
}

void pattern_sparkle(NeoPixel& npx, uint t) {
  if (t % 8)
    return;
  for (uint i = 0; i < npx.num_pixels; ++i)
    npx.setpixel(rand() % 16 ? 0 : 0xffffffff);
}

void pattern_greys(NeoPixel& npx, uint t) {
  uint max = 100; // let's not draw too much current!
  t %= max;
  for (uint i = 0; i < npx.num_pixels; ++i) {
    npx.setpixel(t * 0x1010100);
    if (++t >= max)
      t = 0;
  }
}

std::pair<std::function<void(NeoPixel&, uint t)>, const char*> pattern_table[] = {
    {pattern_snakes, "Snakes!"},
    {pattern_random, "Random data"},
    {pattern_sparkle, "Sparkles"},
    {pattern_greys, "Greys"},
};


int main() {
  // set_sys_clock_48();
  stdio_init_all();
  printf("WS2812 Smoke Test, using pin %d\n", WS2812_PIN);

  NeoPixel npx(WS2812_PIN, NUM_PIXELS);

  set_palette_spectrum(palette);

  int t = 0;
  for (;;) {
    int pat = rand() % count_of(pattern_table);
    int dir = rand() & 1 ? 1 : -1;
    puts(pattern_table[pat].second);
    puts(dir == 1 ? "(forward)" : "(backward)");
    for (int i = 0; i < 1000; ++i) {
      pattern_table[pat].first(npx, t);
      sleep_ms(10);
      t += dir;
    }
  }
}

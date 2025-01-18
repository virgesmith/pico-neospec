#pragma once
#include "hardware/pio.h"

#include <array>
#include <cstdint>

enum { W, B, R, G};
union RGBW {
  RGBW() : u32(0) {}
  RGBW(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) : rgbw{w, b, r, g} {}

  RGBW scaled(float scale);

  uint8_t rgbw[4];
  int32_t u32;
};

const auto RED = RGBW(255, 0, 0);
const auto GREEN = RGBW(0, 255, 0);
const auto BLUE = RGBW(0, 0, 255);

template<size_t N>
void set_palette_red_to_green(std::array<RGBW, N>& palette) {
  const int mult = 255 / N;
  for (int i = 0; i < N; ++i) {
    palette[i].rgbw[R] = mult * (N - i);
    palette[i].rgbw[G] = mult * i;
  }
}

template<size_t N>
void set_palette_spectrum(std::array<RGBW, N>& palette) {
  const int mult = 255 / N;

  for (int i = 0; i < N / 2; ++i) {
    palette[i].rgbw[R] = 4 * (N / 2 - i);
    palette[i].rgbw[G] = 4 * i;
    palette[i].rgbw[B] = 0;
  }
  for (int i = 0; i < N / 2; ++i) {
    palette[i + N / 2].rgbw[R] = 0;
    palette[i + N / 2].rgbw[G] = 4 * (N / 2 - i);
    palette[i + N / 2].rgbw[B] = 4 * i;
  }
}



class NeoPixel final {
public:
  NeoPixel(int pin, int num_pixels);

  NeoPixel(const NeoPixel&) = delete;
  NeoPixel(NeoPixel&&) = delete;
  NeoPixel& operator=(const NeoPixel&) = delete;
  NeoPixel& operator=(NeoPixel&&) = delete;

  ~NeoPixel();

  int setpixel(RGBW rgbw) { return setpixel(rgbw.u32); };
  int setpixel(uint32_t u32);

  int idx() const { return _idx; }

  const int pin;
  const int num_pixels;
private:
  int _idx;
  PIO pio;
  uint sm;
  uint offset;
};

#pragma once

#include "kiss_fft/kiss_fftr.h"

#include "hardware/adc.h"
#include "hardware/dma.h"

#include <array>
#include <cstdint>

class ADCFFT final {
public:
  // sample size should not exceed 9000 (RP2040 memory constraints)
  constexpr static uint SAMPLE_SIZE = 960;

  template <typename T> using array = std::array<T, SAMPLE_SIZE>;

  ADCFFT(uint8_t adcpin, uint sample_freq);

  ADCFFT(const ADCFFT&) = delete;
  ADCFFT& operator=(const ADCFFT&) = delete;
  ADCFFT(ADCFFT&&) = delete;
  ADCFFT& operator=(ADCFFT&&) = delete;

  ~ADCFFT();

  const array<kiss_fft_cpx>& sample_raw();
  const array<float>& frequency_bins() const { return freqs; };

  const uint clock_div;
  const uint sample_freq;

  // ADC Channel 0 is GPIO26
  const uint8_t adc_channel;

private:
  dma_channel_config dma_cfg;
  uint dma_chan;
  array<float> freqs;

  array<uint8_t> capture_buffer;
  array<kiss_fft_scalar> signal; // kiss_fft_scalar is a float
  array<kiss_fft_cpx> fft;
  kiss_fftr_cfg fft_cfg;
};
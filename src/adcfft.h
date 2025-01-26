#pragma once

#include "kiss_fft/kiss_fftr.h"

#include "hardware/adc.h"
#include "hardware/dma.h"

#include <cstdint>
#include <vector>

class ADCFFT final {
public:
  // sample size should not exceed 9000 (RP2040 memory constraints)
  ADCFFT(uint8_t adcpin, uint sample_freq, uint sample_size);

  ~ADCFFT();

  const std::vector<kiss_fft_cpx>& sample_raw();
  const std::vector<float>& frequency_bins() const { return freqs; };

  const uint sample_size;
  const uint clock_div;

  const uint sample_freq;

  // Channel 0 is GPIO26
  const uint8_t adcpin;

private:
  dma_channel_config dma_cfg;
  uint dma_chan;
  std::vector<float> freqs;

  std::vector<uint8_t> capture_buffer;
  std::vector<kiss_fft_scalar> signal; // kiss_fft_scalar is a float
  std::vector<kiss_fft_cpx> fft;
  kiss_fftr_cfg fft_cfg;
};
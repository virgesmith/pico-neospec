#include "kiss_fftr.h"

#include "hardware/adc.h"
#include "hardware/dma.h"

#include <cstdint>

class ADCFFT {
public:
  // BE CAREFUL: anything over about 9000 here will cause things
  // to silently break. The code will compile and upload, but due
  // to memory issues nothing will work properly
  constexpr static uint SAMPLE_SIZE = 960;

  ADCFFT(uint8_t adcpin, uint clock_div, uint SAMPLE_FREQ);

  ~ADCFFT();

  const kiss_fft_cpx* sample_raw();
  const float* frequency_bins() { return freqs; };

  const uint clock_div;
  const uint SAMPLE_FREQ;

  // Channel 0 is GPIO26
  const uint8_t adcpin;

private:
  dma_channel_config dma_cfg;
  uint dma_chan;
  float freqs[SAMPLE_SIZE];

  uint8_t cap_buf[SAMPLE_SIZE];
  kiss_fft_scalar fft_in[SAMPLE_SIZE]; // kiss_fft_scalar is a float
  kiss_fft_cpx fft_out[SAMPLE_SIZE];
  kiss_fftr_cfg fft_cfg;
};
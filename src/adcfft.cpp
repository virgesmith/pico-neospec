#include "adcfft.h"
#include "pico/stdlib.h"

#include <algorithm>
#include <numeric>

namespace {

// set clock_div to determine sample rate
// 0     = 500kHz  (sampling takes 96 cycles)
// 960   = 50kHz
// 9600  = 5kHz
uint get_clock_div(uint sample_freq) {
  // max sample rate @ 48kHz clock is 500kHz
  return (sample_freq >= 500'000) ? 0 : (48'000'000 / sample_freq);
}

} // namespace

ADCFFT::ADCFFT(uint8_t adc_channel, uint sample_freq)
    : adc_channel(adc_channel), sample_freq(sample_freq), clock_div(get_clock_div(sample_freq)) {
  fft_cfg = kiss_fftr_alloc(SAMPLE_SIZE, false, 0, 0);

  adc_gpio_init(26 + adc_channel);

  adc_init();
  adc_select_input(adc_channel);
  adc_fifo_setup(true,  // Write each completed conversion to the sample FIFO
                 true,  // Enable DMA data request (DREQ)
                 1,     // DREQ (and IRQ) asserted when at least 1 sample present
                 false, // We won't see the ERR bit because of 8 bit reads; disable.
                 true   // Shift each sample to 8 bits when pushing to FIFO
  );

  // set sample rate
  adc_set_clkdiv(clock_div);

  sleep_ms(1000);
  // Set up the DMA to start transferring data as soon as it appears in FIFO
  dma_chan = dma_claim_unused_channel(true);
  dma_cfg = dma_channel_get_default_config(dma_chan);

  // Reading from constant address, writing to incrementing byte addresses
  channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_8);
  channel_config_set_read_increment(&dma_cfg, false);
  channel_config_set_write_increment(&dma_cfg, true);

  // Pace transfers based on availability of ADC samples
  channel_config_set_dreq(&dma_cfg, DREQ_ADC);

  // calculate frequencies of each bin
  float dfreq = sample_freq / SAMPLE_SIZE;
  for (size_t i = 0; i < SAMPLE_SIZE; i++) {
    freqs[i] = dfreq * i;
  }
}

ADCFFT::~ADCFFT() { kiss_fft_free(fft_cfg); }

const ADCFFT::array<kiss_fft_cpx>& ADCFFT::sample_raw() {
  adc_fifo_drain();
  adc_run(false);

  dma_channel_configure(dma_chan, &dma_cfg,
                        capture_buffer.data(), // dst
                        &adc_hw->fifo,         // src
                        SAMPLE_SIZE,           // transfer count
                        true                   // start immediately
  );

  adc_run(true);
  dma_channel_wait_for_finish_blocking(dma_chan);

  // fill fourier transform input while subtracting DC component
  float avg = float(std::accumulate(capture_buffer.begin(), capture_buffer.end(), 0)) / capture_buffer.size();
  std::transform(capture_buffer.begin(), capture_buffer.end(), signal.begin(),
                 [avg](uint8_t n) { return float(n) - avg; });

  // compute fast fourier transform
  kiss_fftr(fft_cfg, signal.data(), fft.data());
  return fft;
}

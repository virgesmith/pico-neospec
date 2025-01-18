#include "adcfft.h"
#include "pico/stdlib.h"

ADCFFT::ADCFFT(uint8_t adcpin, uint clock_div, uint SAMPLE_FREQ) : adcpin(adcpin), clock_div(clock_div), SAMPLE_FREQ(SAMPLE_FREQ) {
  fft_cfg = kiss_fftr_alloc(SAMPLE_SIZE, false, 0, 0);

  adc_gpio_init(26 + adcpin);

  adc_init();
  adc_select_input(adcpin);
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
  float f_res = SAMPLE_FREQ / SAMPLE_SIZE;
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    freqs[i] = f_res * i;
  }
}

ADCFFT::~ADCFFT() { kiss_fft_free(fft_cfg); }

const kiss_fft_cpx* ADCFFT::sample_raw() {
  adc_fifo_drain();
  adc_run(false);

  dma_channel_configure(dma_chan, &dma_cfg,
                        cap_buf,       // dst
                        &adc_hw->fifo, // src
                        SAMPLE_SIZE,         // transfer count
                        true           // start immediately
  );

  adc_run(true);
  dma_channel_wait_for_finish_blocking(dma_chan);

  // fill fourier transform input while subtracting DC component
  uint64_t sum = 0;
  for (int i = 0; i < ADCFFT::SAMPLE_SIZE; i++) {
    sum += cap_buf[i];
  }
  float avg = (float)sum / ADCFFT::SAMPLE_SIZE;
  for (int i = 0; i < ADCFFT::SAMPLE_SIZE; i++) {
    fft_in[i] = (float)cap_buf[i] - avg;
  }

  // compute fast fourier transform
  kiss_fftr(fft_cfg, fft_in, fft_out);
  return fft_out;
}



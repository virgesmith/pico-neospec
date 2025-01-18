#include "adcfft.h"

#include "pico/stdlib.h"

#include <cmath>
#include <cstdio>

// set this to determine sample rate
// 0     = 500,000 Hz
// 960   = 50,000 Hz
// 9600  = 5,000 Hz
constexpr uint CLOCK_DIV = 960;
constexpr uint SAMPLE_FREQ = 50000;

// Channel 0 is GPIO26
constexpr uint8_t CAPTURE_CHANNEL = 0;
constexpr uint8_t LED_PIN = 18;

int main() {
  // uint8_t cap_buf[SAMPLE_SIZE];
  // kiss_fft_scalar fft_in[SAMPLE_SIZE]; // kiss_fft_scalar is a float
  // kiss_fft_cpx fft_out[SAMPLE_SIZE];
  // kiss_fftr_cfg cfg = kiss_fftr_alloc(SAMPLE_SIZE, false, 0, 0);

  stdio_init_all();

  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  ADCFFT adcfft(CAPTURE_CHANNEL, CLOCK_DIV, SAMPLE_FREQ);

  const float* freqs = adcfft.frequency_bins();

  auto t = time_us_64();

  for (;;) {
    gpio_put(LED_PIN, 1);
    // get SAMPLE_SIZE samples at SAMPLE_FREQ
    auto fft_out = adcfft.sample_raw();
    gpio_put(LED_PIN, 0);

    // compute power and calculate max freq component
    float max_power = 0;
    int max_idx = 0;
    // any frequency bin over SAMPLE_SIZE/2 is aliased (nyquist sampling theorem)
    for (int i = 0; i < ADCFFT::SAMPLE_SIZE / 2; i++) {
      float power = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
      if (power > max_power) {
        max_power = power;
        max_idx = i;
      }
    }

    auto tnew = time_us_64();
    printf("fpeak=%0.1fHz dt=%ldus\n", freqs[max_idx], tnew - t);
    t = tnew;
    sleep_ms(1000);
  }
}

// void sample(uint8_t* capture_buf) {
//   adc_fifo_drain();
//   adc_run(false);

//   dma_channel_configure(dma_chan, &cfg,
//                         capture_buf,   // dst
//                         &adc_hw->fifo, // src
//                         SAMPLE_SIZE,         // transfer count
//                         true           // start immediately
//   );

//   gpio_put(LED_PIN, 1);
//   adc_run(true);
//   dma_channel_wait_for_finish_blocking(dma_chan);
//   gpio_put(LED_PIN, 0);
// }

// void setup() {

// adc_gpio_init(26 + CAPTURE_CHANNEL);

// adc_init();
// adc_select_input(CAPTURE_CHANNEL);
// adc_fifo_setup(true,  // Write each completed conversion to the sample FIFO
//                true,  // Enable DMA data request (DREQ)
//                1,     // DREQ (and IRQ) asserted when at least 1 sample present
//                false, // We won't see the ERR bit because of 8 bit reads; disable.
//                true   // Shift each sample to 8 bits when pushing to FIFO
// );

// // set sample rate
// adc_set_clkdiv(CLOCK_DIV);

// sleep_ms(1000);
// // Set up the DMA to start transferring data as soon as it appears in FIFO
// uint dma_chan = dma_claim_unused_channel(true);
// cfg = dma_channel_get_default_config(dma_chan);

// // Reading from constant address, writing to incrementing byte addresses
// channel_config_set_transfer_data_size(&cfg, DMA_SIZE_8);
// channel_config_set_read_increment(&cfg, false);
// channel_config_set_write_increment(&cfg, true);

// // Pace transfers based on availability of ADC samples
// channel_config_set_dreq(&cfg, DREQ_ADC);

// // calculate frequencies of each bin
// float f_max = SAMPLE_FREQ;
// float f_res = f_max / SAMPLE_SIZE;
// for (int i = 0; i < SAMPLE_SIZE; i++) {
//   freqs[i] = f_res * i;
// }
// }

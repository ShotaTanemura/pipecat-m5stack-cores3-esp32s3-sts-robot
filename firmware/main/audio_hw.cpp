// ES7210/AW88298 register sequences and I2S pin map adapted from M5Unified
// (MIT License) src/M5Unified.inl @ M5Unified 0.2.23 (this project's pinned version, see
// firmware/dependencies.lock): _microphone_enabled_cb_cores3 (ES7210) and
// _speaker_enabled_cb_cores3 (AW88298 + AW9523 speaker-rail bit), plus the CoreS3
// mic_cfg/spk_cfg pin assignments a few hundred lines above each callback in the same file.
// https://github.com/m5stack/M5Unified

#include "audio_hw.h"

#include <driver/i2s_std.h>

#include "esp_err.h"
#include "esp_log.h"
#include "main.h"

// Must match media.cpp's Opus SAMPLE_RATE -- both describe the same physical audio stream.
#define AUDIO_HW_SAMPLE_RATE (16000)

// CoreS3 I2S_NUM_1 pin map (M5Unified.inl mic_cfg/spk_cfg for board_M5StackCoreS3).
#define PIN_MCLK GPIO_NUM_0
#define PIN_BCLK GPIO_NUM_34
#define PIN_WS GPIO_NUM_33
#define PIN_DOUT GPIO_NUM_13  // to AW88298 (speaker)
#define PIN_DIN GPIO_NUM_14   // from ES7210 (mic)

// M5Unified's own net software gain for this exact board config is 1.0x, not 2x:
// Mic_Class.inl computes f_gain = mic_cfg.magnification / (mic_cfg.over_sampling << 1),
// and CoreS3 sets magnification=2, over_sampling=1 -> 2/2 = 1.0. Match that here rather
// than introduce an unreviewed loudness change; ES7210 hardware gain (MIC1_GAIN/
// MIC2_GAIN below) is unchanged from M5Unified's values regardless.
#define MIC_GAIN 1

static const uint8_t AW9523_I2C_ADDR = 0x58;
static const uint8_t ES7210_I2C_ADDR = 0x40;
static const uint8_t AW88298_I2C_ADDR = 0x36;
static const uint32_t CODEC_I2C_FREQ = 400000;

static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

static void aw88298_write_reg(uint8_t reg, uint16_t value) {
  // AW88298 registers are big-endian on the wire; M5Unified byte-swaps before writing.
  value = __builtin_bswap16(value);
  M5.In_I2C.writeRegister(AW88298_I2C_ADDR, reg, (const uint8_t *)&value, 2,
                         CODEC_I2C_FREQ);
}

static void es7210_write_reg(uint8_t reg, uint8_t value) {
  M5.In_I2C.writeRegister(ES7210_I2C_ADDR, reg, &value, 1, CODEC_I2C_FREQ);
}

static void es7210_init() {
  struct __attribute__((packed)) reg_data_t {
    uint8_t reg;
    uint8_t value;
  };
  es7210_write_reg(0x00, 0xFF);  // RESET_CTL
  static constexpr reg_data_t data[] = {
      {0x00, 0x41},  // RESET_CTL
      {0x01, 0x1f},  // CLK_ON_OFF
      {0x06, 0x00},  // DIGITAL_PDN
      {0x07, 0x20},  // ADC_OSR
      {0x08, 0x10},  // MODE_CFG
      {0x09, 0x30},  // TCT0_CHPINI
      {0x0A, 0x30},  // TCT1_CHPINI
      {0x20, 0x0a},  // ADC34_HPF2
      {0x21, 0x2a},  // ADC34_HPF1
      {0x22, 0x0a},  // ADC12_HPF2
      {0x23, 0x2a},  // ADC12_HPF1
      {0x02, 0xC1},
      {0x04, 0x01},
      {0x05, 0x00},
      {0x11, 0x60},
      {0x40, 0x42},  // ANALOG_SYS
      {0x41, 0x70},  // MICBIAS12
      {0x42, 0x70},  // MICBIAS34
      {0x43, 0x1B},  // MIC1_GAIN
      {0x44, 0x1B},  // MIC2_GAIN
      {0x45, 0x00},  // MIC3_GAIN
      {0x46, 0x00},  // MIC4_GAIN
      {0x47, 0x00},  // MIC1_LP
      {0x48, 0x00},  // MIC2_LP
      {0x49, 0x00},  // MIC3_LP
      {0x4A, 0x00},  // MIC4_LP
      {0x4B, 0x00},  // MIC12_PDN
      {0x4C, 0xFF},  // MIC34_PDN -- MIC3/4 unused on CoreS3, no echo-reference channel
      {0x01, 0x14},  // CLK_ON_OFF
  };
  for (auto &d : data) {
    es7210_write_reg(d.reg, d.value);
  }
}

static void aw88298_init() {
  // Powers the AW88298 3.3V rail via the AW9523 GPIO expander (port0 bit 2).
  M5.In_I2C.bitOn(AW9523_I2C_ADDR, 0x02, 0b00000100, CODEC_I2C_FREQ);

  // Sample-rate field (reg 0x06 low bits): same table/derivation as M5Unified so the
  // constant stays traceable to a formula instead of a magic number.
  static constexpr uint8_t rate_tbl[] = {4, 5, 6, 8, 10, 11, 15, 20, 22, 44};
  size_t reg0x06_value = 0;
  size_t rate = (AUDIO_HW_SAMPLE_RATE + 1102) / 2205;
  while (rate > rate_tbl[reg0x06_value] &&
        ++reg0x06_value < sizeof(rate_tbl)) {
  }
  reg0x06_value |= 0x14C0;  // I2SBCK=0 (BCK mode 16*2)

  aw88298_write_reg(0x61, 0x0673);       // boost mode disabled
  aw88298_write_reg(0x04, 0x4040);       // I2SEN=1 AMPPD=0 PWDN=0
  aw88298_write_reg(0x05, 0x0008);       // RMSE=0 HAGCE=0 HDCCE=0 HMUTE=0
  aw88298_write_reg(0x06, reg0x06_value);
  aw88298_write_reg(0x0C, 0x0064);       // volume setting (full volume)
}

static void i2s_full_duplex_init() {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));

  i2s_std_gpio_config_t gpio_cfg = {
      .mclk = PIN_MCLK,
      .bclk = PIN_BCLK,
      .ws = PIN_WS,
      .dout = PIN_DOUT,
      .din = PIN_DIN,
      .invert_flags =
          {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
  };

  // ESP-IDF's full-duplex TX/RX pair shares BCLK/WS, and its own docs require both
  // channels use the same std_cfg (including slot_cfg) in that mode -- so both are
  // stereo here, even though the AW88298 (speaker) only physically wires up one data
  // line: pipecat_audio_hw_write() duplicates its mono sample into both slots, and
  // pipecat_audio_hw_read() mixes both of the mic's two slots (MIC1 + MIC2) down to mono.
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_HW_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = gpio_cfg,
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void pipecat_audio_hw_init() {
  // ALDO1/ALDO2 (AW88298/ES7210 analog rails) are already enabled by M5.begin()
  // (Power_Class); only the AW9523 speaker-enable bit is ours to set, in aw88298_init().
  i2s_full_duplex_init();
  es7210_init();
  aw88298_init();
}

// Largest frame this project ever asks for: media.cpp's PCM_BUFFER_SIZE / sizeof(int16_t).
#define AUDIO_HW_MAX_FRAME_SAMPLES 320

// i2s_channel_read/write take a millisecond timeout, not a tick count -- a 20ms frame at
// 16 kHz should always be ready well inside this.
#define I2S_TIMEOUT_MS 1000

void pipecat_audio_hw_read(int16_t *out, size_t samples) {
  // One stereo frame is 2 int16_t (L=MIC1, R=MIC2). Mix down like M5Unified's own mono
  // capture path does for this board (Mic_Class.inl's in_stereo branch: average, then
  // gain, then clamp) rather than discarding MIC2 and risking int16 wraparound on a
  // plain multiply.
  static int16_t stereo_buf[AUDIO_HW_MAX_FRAME_SAMPLES * 2];
  size_t bytes_read = 0;
  i2s_channel_read(rx_handle, stereo_buf, samples * 2 * sizeof(int16_t),
                   &bytes_read, I2S_TIMEOUT_MS);
  size_t frames_read = bytes_read / (2 * sizeof(int16_t));
  for (size_t i = 0; i < frames_read; i++) {
    int32_t mixed = ((int32_t)stereo_buf[i * 2] + (int32_t)stereo_buf[i * 2 + 1] + 1) >> 1;
    int32_t amplified = mixed * MIC_GAIN;
    if (amplified > INT16_MAX) {
      amplified = INT16_MAX;
    } else if (amplified < INT16_MIN) {
      amplified = INT16_MIN;
    }
    out[i] = (int16_t)amplified;
  }
  for (size_t i = frames_read; i < samples; i++) {
    out[i] = 0;
  }
}

void pipecat_audio_hw_write(const int16_t *in, size_t samples) {
  // The TX channel is configured stereo to match RX (ESP-IDF requires identical
  // std_cfg -- including slot_cfg -- for a full-duplex TX/RX pair sharing BCLK/WS);
  // the AW88298 only wires up one physical data line, so duplicate the mono sample
  // into both slots.
  static int16_t stereo_buf[AUDIO_HW_MAX_FRAME_SAMPLES * 2];
  for (size_t i = 0; i < samples; i++) {
    stereo_buf[i * 2] = in[i];
    stereo_buf[i * 2 + 1] = in[i];
  }
  size_t bytes_written = 0;
  i2s_channel_write(tx_handle, stereo_buf, samples * 2 * sizeof(int16_t),
                    &bytes_written, I2S_TIMEOUT_MS);
}

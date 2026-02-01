#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#include <SPI.h>
#else
#include "driver/spi_master.h"
#include "esp_err.h"
#endif

#include "roo_display/hal/spi_settings.h"
#include "roo_io/data/byte_order.h"
#include "soc/spi_reg.h"

#if CONFIG_IDF_TARGET_ESP32
#define ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT 3
#else
#define ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT 2
#ifndef SPI_MOSI_DLEN_REG
#define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)
#endif
#ifndef SPI_MISO_DLEN_REG
#define SPI_MISO_DLEN_REG(x) SPI_MS_DLEN_REG(x)
#endif
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2)
#define ROO_DISPLAY_SPI_CMD_UPDATE_REQUIRED 1
#else
#define ROO_DISPLAY_SPI_CMD_UPDATE_REQUIRED 0
#endif

#ifndef ROO_TESTING

namespace roo_display {
namespace esp32 {

inline void SpiTxWait(uint8_t spi_port) __attribute__((always_inline));
inline void SpiTxStart(uint8_t spi_port) __attribute__((always_inline));

inline void SpiTxWait(uint8_t spi_port) {
  while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR) {
  }
}

inline void SpiTxStart(uint8_t spi_port) {
#if ROO_DISPLAY_SPI_CMD_UPDATE_REQUIRED
  SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_UPDATE);
  while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_UPDATE) {
  }
#endif

  // SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
  // The 'correct' way to set the SPI_USR would be to set just the single bit,
  // as in the commented-out code above. But the remaining bits of this register
  // are unused (marked 'reserved'; see
  // https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
  // page 131). Unconditionally setting the entire register brings significant
  // performance improvements.
  WRITE_PERI_REG(SPI_CMD_REG(spi_port), SPI_USR);
}

template <uint8_t spi_port, typename SpiSettings>
class Esp32SpiDevice;

template <uint8_t spi_port>
class Esp32Spi {
 public:
  template <typename SpiSettings>
  using Device = Esp32SpiDevice<spi_port, SpiSettings>;

#if defined(ARDUINO)
  Esp32Spi() : spi_(SPI) {
    static_assert(
        spi_port == ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT,
        "When using a SPI interface different than the default, you must "
        "provide a SPIClass object also in the constructor.");
  }

  Esp32Spi(decltype(SPI)& spi) : spi_(spi) {}

  void init() { spi_.begin(); }

  void init(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_.begin(sck, miso, mosi);
  }

#else
  // The host enum is one-off from the spi_port.
  Esp32Spi() : spi_((spi_host_device_t)(spi_port - 1)) {}

  void init() {
    spi_bus_config_t config = {
        .mosi_io_num = -1,
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(spi_, &config, SPI_DMA_CH_AUTO));
  }

  void init(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_bus_config_t config = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(spi_, &config, SPI_DMA_CH_AUTO));
  }

#endif

 private:
  template <uint8_t, typename SpiSettings>
  friend class Esp32SpiDevice;

#if defined(ARDUINO)
  decltype(SPI)& spi_;
#else
  spi_host_device_t spi_;
#endif
};

template <uint8_t spi_port, typename SpiSettings>
class Esp32SpiDevice {
 public:
  Esp32SpiDevice(Esp32Spi<spi_port>& spi) : spi_(spi.spi_) {}

#if defined(ARDUINO)
  void init() {}

  void beginReadWriteTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
  }

  void beginWriteOnlyTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
    // Enable write-only mode.
    WRITE_PERI_REG(SPI_USER_REG(spi_port), SPI_USR_MOSI);
  }

  void endTransaction() {
    // Re-enable read-write mode.
    WRITE_PERI_REG(SPI_USER_REG(spi_port),
                   SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN);
    spi_.endTransaction();
  }
#else
  void init() {
    spi_device_interface_config_t config_ = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = SpiSettings::data_mode,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = SpiSettings::clock,
        .input_delay_ns = 0,
        .spics_io_num = -1,
        .flags = {SpiSettings::bit_order == kSpiLsbFirst
                      ? SPI_DEVICE_BIT_LSBFIRST
                      : 0},
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(spi_, &config_, &device_));
  }

  void beginReadWriteTransaction() {
    spi_device_acquire_bus(device_, portMAX_DELAY);
  }

  void beginWriteOnlyTransaction() {
    spi_device_acquire_bus(device_, portMAX_DELAY);
    // Enable write-only mode.
    WRITE_PERI_REG(SPI_USER_REG(spi_port), SPI_USR_MOSI);
  }

  void endTransaction() {
    // Re-enable read-write mode.
    WRITE_PERI_REG(SPI_USER_REG(spi_port),
                   SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN);
    spi_device_release_bus(device_);
  }
#endif

  void sync() __attribute__((always_inline)) {
    // if (need_sync_)
    SpiTxWait(spi_port);
    need_sync_ = false;
  }

  void write(uint8_t data) __attribute__((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 7);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }

  void write16(uint16_t data) __attribute__((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 15);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), roo_io::htobe(data));
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }

  void write16x2(uint16_t a, uint16_t b) __attribute((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
    WRITE_PERI_REG(SPI_W0_REG(spi_port),
                   roo_io::htobe(a) | (roo_io::htobe(b) << 16));
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }

  void write16x2_async(uint16_t a, uint16_t b) __attribute((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
    WRITE_PERI_REG(SPI_W0_REG(spi_port),
                   roo_io::htobe(a) | (roo_io::htobe(b) << 16));
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  void write16be(uint16_t data) __attribute((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 15);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }

  void write32(uint32_t data) __attribute__((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), roo_io::htobe(data));
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }

  void write32be(uint32_t data) __attribute__((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }

  void writeBytes_async(uint8_t* data, uint32_t len) {
    uint32_t* d32 = (uint32_t*)data;
    if (len >= 64) {
      WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 511);
      while (true) {
        WRITE_PERI_REG(SPI_W0_REG(spi_port), d32[0]);
        WRITE_PERI_REG(SPI_W1_REG(spi_port), d32[1]);
        WRITE_PERI_REG(SPI_W2_REG(spi_port), d32[2]);
        WRITE_PERI_REG(SPI_W3_REG(spi_port), d32[3]);
        WRITE_PERI_REG(SPI_W4_REG(spi_port), d32[4]);
        WRITE_PERI_REG(SPI_W5_REG(spi_port), d32[5]);
        WRITE_PERI_REG(SPI_W6_REG(spi_port), d32[6]);
        WRITE_PERI_REG(SPI_W7_REG(spi_port), d32[7]);
        WRITE_PERI_REG(SPI_W8_REG(spi_port), d32[8]);
        WRITE_PERI_REG(SPI_W9_REG(spi_port), d32[9]);
        WRITE_PERI_REG(SPI_W10_REG(spi_port), d32[10]);
        WRITE_PERI_REG(SPI_W11_REG(spi_port), d32[11]);
        WRITE_PERI_REG(SPI_W12_REG(spi_port), d32[12]);
        WRITE_PERI_REG(SPI_W13_REG(spi_port), d32[13]);
        WRITE_PERI_REG(SPI_W14_REG(spi_port), d32[14]);
        WRITE_PERI_REG(SPI_W15_REG(spi_port), d32[15]);
        SpiTxStart(spi_port);
        len -= 64;
        d32 += 16;
        SpiTxWait(spi_port);
        if (len < 64) break;
      }
    }
    if (len == 0) return;
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
    do {
      WRITE_PERI_REG(SPI_W0_REG(spi_port), d32[0]);
      if (len <= 4) break;
      WRITE_PERI_REG(SPI_W1_REG(spi_port), d32[1]);
      if (len <= 8) break;
      WRITE_PERI_REG(SPI_W2_REG(spi_port), d32[2]);
      if (len <= 12) break;
      WRITE_PERI_REG(SPI_W3_REG(spi_port), d32[3]);
      if (len <= 16) break;
      WRITE_PERI_REG(SPI_W4_REG(spi_port), d32[4]);
      if (len <= 20) break;
      WRITE_PERI_REG(SPI_W5_REG(spi_port), d32[5]);
      if (len <= 24) break;
      WRITE_PERI_REG(SPI_W6_REG(spi_port), d32[6]);
      if (len <= 28) break;
      WRITE_PERI_REG(SPI_W7_REG(spi_port), d32[7]);
      if (len <= 32) break;
      WRITE_PERI_REG(SPI_W8_REG(spi_port), d32[8]);
      if (len <= 36) break;
      WRITE_PERI_REG(SPI_W9_REG(spi_port), d32[9]);
      if (len <= 40) break;
      WRITE_PERI_REG(SPI_W10_REG(spi_port), d32[10]);
      if (len <= 44) break;
      WRITE_PERI_REG(SPI_W11_REG(spi_port), d32[11]);
      if (len <= 48) break;
      WRITE_PERI_REG(SPI_W12_REG(spi_port), d32[12]);
      if (len <= 52) break;
      WRITE_PERI_REG(SPI_W13_REG(spi_port), d32[13]);
      if (len <= 56) break;
      WRITE_PERI_REG(SPI_W14_REG(spi_port), d32[14]);
      if (len <= 60) break;
      WRITE_PERI_REG(SPI_W15_REG(spi_port), d32[15]);
    } while (false);
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  void fill16_async(uint16_t data, uint32_t len)
      __attribute__((always_inline)) {
    fill16be_async(roo_io::htobe(data), len);
  }

  void fill16be_async(uint16_t data, uint32_t len) {
    uint32_t d32 = (data << 16) | data;
    len *= 2;
    bool large = (len >= 64);
    if (large) {
      WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 511);
      WRITE_PERI_REG(SPI_W0_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W1_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W2_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W3_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W4_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W5_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W6_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W7_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W8_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W9_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W10_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W11_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W12_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W13_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W14_REG(spi_port), d32);
      WRITE_PERI_REG(SPI_W15_REG(spi_port), d32);
      while (true) {
        SpiTxStart(spi_port);
        SpiTxWait(spi_port);
        len -= 64;
        if (len < 64) break;
      }
    }
    if (len == 0) return;
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
    do {
      if (large) break;
      WRITE_PERI_REG(SPI_W0_REG(spi_port), d32);
      if (len <= 4) break;
      WRITE_PERI_REG(SPI_W1_REG(spi_port), d32);
      if (len <= 8) break;
      WRITE_PERI_REG(SPI_W2_REG(spi_port), d32);
      if (len <= 12) break;
      WRITE_PERI_REG(SPI_W3_REG(spi_port), d32);
      if (len <= 16) break;
      WRITE_PERI_REG(SPI_W4_REG(spi_port), d32);
      if (len <= 20) break;
      WRITE_PERI_REG(SPI_W5_REG(spi_port), d32);
      if (len <= 24) break;
      WRITE_PERI_REG(SPI_W6_REG(spi_port), d32);
      if (len <= 28) break;
      WRITE_PERI_REG(SPI_W7_REG(spi_port), d32);
      if (len <= 32) break;
      WRITE_PERI_REG(SPI_W8_REG(spi_port), d32);
      if (len <= 36) break;
      WRITE_PERI_REG(SPI_W9_REG(spi_port), d32);
      if (len <= 40) break;
      WRITE_PERI_REG(SPI_W10_REG(spi_port), d32);
      if (len <= 44) break;
      WRITE_PERI_REG(SPI_W11_REG(spi_port), d32);
      if (len <= 48) break;
      WRITE_PERI_REG(SPI_W12_REG(spi_port), d32);
      if (len <= 52) break;
      WRITE_PERI_REG(SPI_W13_REG(spi_port), d32);
      if (len <= 56) break;
      WRITE_PERI_REG(SPI_W14_REG(spi_port), d32);
      if (len <= 60) break;
      WRITE_PERI_REG(SPI_W15_REG(spi_port), d32);
    } while (false);
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  void fill24be_async(uint32_t data, uint32_t len) {
    const uint8_t* buf = (const uint8_t*)(&data);
    uint32_t r = buf[1];
    uint32_t g = buf[2];
    uint32_t b = buf[3];
    // Concatenate 4 pixels into three 32 bit blocks.
    uint32_t d2 = b | r << 8 | g << 16 | b << 24;
    uint32_t d1 = d2 << 8 | g;
    uint32_t d0 = d1 << 8 | r;
    len *= 3;
    bool large = (len >= 60);
    if (large) {
      WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 479);
      WRITE_PERI_REG(SPI_W0_REG(spi_port), d0);
      WRITE_PERI_REG(SPI_W1_REG(spi_port), d1);
      WRITE_PERI_REG(SPI_W2_REG(spi_port), d2);
      WRITE_PERI_REG(SPI_W3_REG(spi_port), d0);
      WRITE_PERI_REG(SPI_W4_REG(spi_port), d1);
      WRITE_PERI_REG(SPI_W5_REG(spi_port), d2);
      WRITE_PERI_REG(SPI_W6_REG(spi_port), d0);
      WRITE_PERI_REG(SPI_W7_REG(spi_port), d1);
      WRITE_PERI_REG(SPI_W8_REG(spi_port), d2);
      WRITE_PERI_REG(SPI_W9_REG(spi_port), d0);
      WRITE_PERI_REG(SPI_W10_REG(spi_port), d1);
      WRITE_PERI_REG(SPI_W11_REG(spi_port), d2);
      WRITE_PERI_REG(SPI_W12_REG(spi_port), d0);
      WRITE_PERI_REG(SPI_W13_REG(spi_port), d1);
      WRITE_PERI_REG(SPI_W14_REG(spi_port), d2);
      do {
        SpiTxStart(spi_port);
        len -= 60;
        SpiTxWait(spi_port);
      } while (len >= 60);
    }
    if (len == 0) return;
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
    do {
      if (large) break;
      WRITE_PERI_REG(SPI_W0_REG(spi_port), d0);
      if (len <= 4) break;
      WRITE_PERI_REG(SPI_W1_REG(spi_port), d1);
      if (len <= 8) break;
      WRITE_PERI_REG(SPI_W2_REG(spi_port), d2);
      if (len <= 12) break;
      WRITE_PERI_REG(SPI_W3_REG(spi_port), d0);
      if (len <= 16) break;
      WRITE_PERI_REG(SPI_W4_REG(spi_port), d1);
      if (len <= 20) break;
      WRITE_PERI_REG(SPI_W5_REG(spi_port), d2);
      if (len <= 24) break;
      WRITE_PERI_REG(SPI_W6_REG(spi_port), d0);
      if (len <= 28) break;
      WRITE_PERI_REG(SPI_W7_REG(spi_port), d1);
      if (len <= 32) break;
      WRITE_PERI_REG(SPI_W8_REG(spi_port), d2);
      if (len <= 36) break;
      WRITE_PERI_REG(SPI_W9_REG(spi_port), d0);
      if (len <= 40) break;
      WRITE_PERI_REG(SPI_W10_REG(spi_port), d1);
      if (len <= 44) break;
      WRITE_PERI_REG(SPI_W11_REG(spi_port), d2);
      if (len <= 48) break;
      WRITE_PERI_REG(SPI_W12_REG(spi_port), d0);
      if (len <= 52) break;
      WRITE_PERI_REG(SPI_W13_REG(spi_port), d1);
      if (len <= 56) break;
      WRITE_PERI_REG(SPI_W14_REG(spi_port), d2);
    } while (false);
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  uint8_t transfer(uint8_t data) __attribute__((always_inline)) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 7);
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
    WRITE_PERI_REG(SPI_MISO_DLEN_REG(spi_port), 7);
#endif
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
    return READ_PERI_REG(SPI_W0_REG(spi_port)) & 0xFF;
  }

  uint16_t transfer16(uint16_t data) __attribute__((always_inline)) {
    // Apply byte-swapping for MSBFIRST (wr_bit_order = 0)
    data = roo_io::htobe(data);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 15);
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
    WRITE_PERI_REG(SPI_MISO_DLEN_REG(spi_port), 15);
#endif
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
    uint16_t result = READ_PERI_REG(SPI_W0_REG(spi_port)) & 0xFFFF;
    // Apply byte-swapping for MSBFIRST (rd_bit_order = 0)
    return roo_io::betoh(result);
  }

  uint32_t transfer32(uint32_t data) __attribute__((always_inline)) {
    // Apply byte-swapping for MSBFIRST (wr_bit_order = 0)
    data = roo_io::htobe(data);
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
    WRITE_PERI_REG(SPI_MISO_DLEN_REG(spi_port), 31);
#endif
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
    uint32_t result = READ_PERI_REG(SPI_W0_REG(spi_port));
    // Apply byte-swapping for MSBFIRST (rd_bit_order = 0)
    return roo_io::betoh(result);
  }

 private:
#if defined(ARDUINO)
  decltype(SPI)& spi_;
#else
  spi_host_device_t spi_;
  spi_device_handle_t device_;
#endif
  bool need_sync_ = false;
};

// Original ESP32: SPI0 (none), FSPI -> SPI1, HSPI -> SPI2, VSPI -> SPI3.
// ESP32S2/S3/P4: FSPI -> SPI2, HSPI -> SPI3.
// Others: FSPI -> SPI2.
//
// Based on
// https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-spi.h
// and
// https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-spi.c.
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || \
    CONFIG_IDF_TARGET_ESP32P4
using Fspi = Esp32Spi<2>;
using Hspi = Esp32Spi<3>;
#elif CONFIG_IDF_TARGET_ESP32
using Fspi = Esp32Spi<1>;
using Hspi = Esp32Spi<2>;
using Vspi = Esp32Spi<3>;
#else  // ESP32C2, C3, C5, C6, C61, H2
using Fspi = Esp32Spi<2>;
#endif

}  // namespace esp32
}  // namespace roo_display

#else

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
namespace esp32 {

#if CONFIG_IDF_TARGET_ESP32
using Vspi = ArduinoSpi;
using Hspi = ArduinoSpi;
#endif
using Fspi = ArduinoSpi;

}  // namespace esp32
}  // namespace roo_display

#endif

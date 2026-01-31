#pragma once

#include <Arduino.h>
#include <SPI.h>

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

// ESP32C3 and S3 require an update signal after writing FIFO/DMA buffers.
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S3
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

template <uint8_t spi_port>
class SpiTransport {
 public:
  SpiTransport() : spi_(SPI) {
    static_assert(
        spi_port == ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT,
        "When using a SPI interface different than the default, you must "
        "provide a SPIClass object also in the constructor.");
  }

  SpiTransport(decltype(SPI)& spi) : spi_(spi) {}

  void init() {}

  void beginReadWriteTransaction(const SPISettings& settings) {
    spi_.beginTransaction(settings);
  }

  void beginWriteOnlyTransaction(const SPISettings& settings) {
    spi_.beginTransaction(settings);
    // Enable write-only mode.
    WRITE_PERI_REG(SPI_USER_REG(spi_port), SPI_USR_MOSI);
  }

  void endTransaction() {
    // Re-enable read-write mode.
    WRITE_PERI_REG(SPI_USER_REG(spi_port),
                   SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN);
    spi_.endTransaction();
  }

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
  decltype(SPI)& spi_;
  bool need_sync_ = false;
};

#if CONFIG_IDF_TARGET_ESP32
using Vspi = SpiTransport<0>;
using Hspi = SpiTransport<1>;
#endif
using Fspi = SpiTransport<2>;

}  // namespace esp32
}  // namespace roo_display

#else

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
namespace esp32 {

#if CONFIG_IDF_TARGET_ESP32
using Vspi = GenericSpi;
using Hspi = GenericSpi;
#endif
using Fspi = GenericSpi;

}  // namespace esp32
}  // namespace roo_display

#endif

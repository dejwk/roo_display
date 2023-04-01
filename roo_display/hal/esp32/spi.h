#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/internal/byte_order.h"
#include "soc/spi_reg.h"

namespace roo_display {
namespace esp32 {

template <uint8_t spi_port>
class SpiTransport {
 public:
  SpiTransport() : spi_(SPI) {
    static_assert(spi_port == VSPI,
                  "When using a SPI interface different than VSPI, you must "
                  "provide a SPIClass object also in the constructor.");
  }

  SpiTransport(decltype(SPI)& spi) : spi_(spi) {}

  void beginTransaction(const SPISettings& settings) {
    spi_.beginTransaction(settings);
  }
  void endTransaction() { spi_.endTransaction(); }

  void writeBytes(uint8_t* data, uint32_t len) {
    uint32_t* d32 = (uint32_t*)data;
    if (len >= 64) {
      WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 511);
      while (len >= 64) {
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
        SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
        len -= 64;
        d32 += 16;
        while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
          ;
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
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void write(uint8_t data) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 7);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void write16(uint16_t data) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 15);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), byte_order::htobe(data));
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void write16be(uint16_t data) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 15);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void write32(uint32_t data) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), byte_order::htobe(data));
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void write32be(uint32_t data) {
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 31);
    WRITE_PERI_REG(SPI_W0_REG(spi_port), data);
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void fill16(uint16_t data, uint32_t len) {
    fill16be(byte_order::htobe(data), len);
  }

  void fill16be(uint16_t data, uint32_t len) {
    uint32_t d32 = (data << 16) | data;
    len *= 2;
    if (len >= 64) {
      WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 511);
      while (len >= 64) {
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
        SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
        len -= 64;
        while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
          ;
      }
    }
    if (len == 0) return;
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
    do {
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
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  void fill24be(uint32_t data, uint32_t len) {
    const uint8_t* buf = (const uint8_t*)(&data);
    uint32_t r = buf[1];
    uint32_t g = buf[2];
    uint32_t b = buf[3];
    // Concatenate 4 pixels into three 32 bit blocks.
    uint32_t d2 = b | r << 8 | g << 16 | b << 24;
    uint32_t d1 = d2 << 8 | g;
    uint32_t d0 = d1 << 8 | r;
    len *= 3;
    if (len >= 60) {
      WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), 479);
      while (len >= 60) {
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
        SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
        len -= 60;
        while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
          ;
      }
    }
    if (len == 0) return;
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
    do {
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
    SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR)
      ;
  }

  uint8_t transfer(uint8_t data) { return spi_.transfer(data); }
  uint16_t transfer16(uint16_t data) { return spi_.transfer16(data); }
  uint32_t transfer32(uint32_t data) { return spi_.transfer32(data); }

 private:
  decltype(SPI)& spi_;
};

using Vspi = SpiTransport<VSPI>;
using Hspi = SpiTransport<HSPI>;
using Fspi = SpiTransport<FSPI>;

}  // namespace esp32

using DefaultSpi = esp32::Vspi;

}  // namespace roo_display

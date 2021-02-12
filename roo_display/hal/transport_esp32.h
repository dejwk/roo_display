#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/internal/byte_order.h"
#include "soc/spi_reg.h"

namespace roo_display {
namespace esp32 {

template <uint8_t spi_port, typename SpiSettings>
class SpiTransport {
 public:
  SpiTransport() : spi_(SPI) {
    static_assert(spi_port == VSPI,
                  "When using a SPI interface different than VSPI, you must "
                  "provide a SPIClass object also in the constructor.");
  }

  SpiTransport(decltype(SPI)& spi) : spi_(spi) {}

  void beginTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
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

 private:
  decltype(SPI)& spi_;
};

struct Vspi {
  template <typename SpiSettings>
  using Transport = SpiTransport<VSPI, SpiSettings>;
};

struct Hspi {
  template <typename SpiSettings>
  using Transport = SpiTransport<HSPI, SpiSettings>;
};

struct Fspi {
  template <typename SpiSettings>
  using Transport = SpiTransport<FSPI, SpiSettings>;
};

}  // namespace esp32
}  // namespace roo_display

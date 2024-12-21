#pragma once

#include <Arduino.h>

#if (defined(ESP32) && (CONFIG_IDF_TARGET_ESP32) && !defined(ROO_TESTING))

#include "roo_display/hal/esp32/spi.h"
#include "roo_io/memory/fill.h"

#else

// Generic Arduino implementation.

#include <SPI.h>

#include "roo_display/internal/byte_order.h"

namespace roo_display {

// SPI transport, using the default <SPI.h> from the Arduino framework.
class GenericSpi {
 public:
  // Creates the SPI transport, with specified transaction settings, using the
  // provided SPI bus.
  GenericSpi(decltype(SPI) & spi) : spi_(spi) {}

  // // Creates the SPI transport, with specified clock, and using MSBFIRST and
  // // SPI_MODE0, using the provided SPI bus.
  // GenericSpi(decltype(SPI)& spi, uint32_t clock)
  //     : spi_(spi), settings_(clock, MSBFIRST, SPI_MODE0) {}

  // // Creates the SPI transport, with specified transaction settings, using
  // the
  // // default SPI bus.
  // GenericSpi(SPISettings settings)
  //     : spi_(SPI), settings_(std::move(settings)) {}

  // // Creates the SPI transport, with specified clock, and using MSBFIRST and
  // // SPI_MODE0, using the default SPI bus.
  // GenericSpi(uint32_t clock)
  //     : spi_(SPI), settings_(clock, MSBFIRST, SPI_MODE0) {}

  // // Creates the SPI transport, with specified transaction settings, using
  // the
  // // default SPI bus.
  // GenericSpi(SPISettings settings)
  //     : spi_(SPI), settings_(std::move(settings)) {}

  GenericSpi() : GenericSpi(SPI) {}

  void beginReadWriteTransaction(SPISettings& settings) {
    spi_.beginTransaction(settings);
  }

  void beginWriteOnlyTransaction(SPISettings& settings) {
    spi_.beginTransaction(settings);
  }

  void endTransaction() { spi_.endTransaction(); }

  void sync() {}

  void write(uint8_t data) { spi_.write(data); }

  void write16(uint16_t data) { spi_.write16(data); }

  void write16x2(uint16_t a, uint16_t b) {
    spi_.write16(a);
    spi_.write16(b);
  }

  void write16x2_async(uint16_t a, uint16_t b) {
    spi_.write16(a);
    spi_.write16(b);
  }

  void write16be(uint16_t data) { spi_.writeBytes((uint8_t*)&data, 2); }

  void write32(uint32_t data) { spi_.write32(data); }

  void write32be(uint32_t data) { spi_.writeBytes((uint8_t*)&data, 4); }

  void writeBytes_async(uint8_t* data, uint32_t len) {
    spi_.writeBytes(data, len);
  }

  void fill16_async(uint16_t data, uint32_t len) {
    fill16be_async(byte_order::htobe(data), len);
  }

  void fill16be_async(uint16_t data, uint32_t len) {
    uint8_t buf[64];
    if (len >= 32) {
      roo_io::PatternFill<2>((roo_io::byte*)buf, 32,
                             (const roo_io::byte*)&data);
      while (len >= 32) {
        spi_.writeBytes(buf, 64);
        len -= 32;
      }
      spi_.writeBytes(buf, len * 2);
      return;
    }
    roo_io::PatternFill<2>((roo_io::byte*)buf, len, (const roo_io::byte*)&data);
    spi_.writeBytes(buf, len * 2);
  }

  void fill24be_async(uint32_t data, uint32_t len) {
    uint8_t buf[96];
    if (len >= 32) {
      roo_io::PatternFill<3>((roo_io::byte*)buf, 32,
                             ((const roo_io::byte*)&data + 1));
      while (len >= 32) {
        spi_.writeBytes(buf, 96);
        len -= 32;
      }
      spi_.writeBytes(buf, len * 3);
      return;
    }
    roo_io::PatternFill<3>((roo_io::byte*)buf, len,
                           ((const roo_io::byte*)&data + 1));
    spi_.writeBytes(buf, len * 3);
  }

  uint8_t transfer(uint8_t data) { return spi_.transfer(data); }
  uint16_t transfer16(uint16_t data) { return spi_.transfer16(data); }
  uint32_t transfer32(uint32_t data) { return spi_.transfer32(data); }

 private:
  decltype(SPI) & spi_;
};

using DefaultSpi = GenericSpi;

}  // namespace roo_display

#endif

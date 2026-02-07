#pragma once

#include <Arduino.h>

// Generic Arduino implementation.

#include <SPI.h>

#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/internal/byte_order.h"
#include "roo_io/data/byte_order.h"
#include "roo_io/memory/fill.h"

namespace roo_display {

template <typename SpiSettings>
class ArduinoSpiDevice;

class ArduinoSpi {
 public:
  template <typename SpiSettings>
  using Device = ArduinoSpiDevice<SpiSettings>;

  ArduinoSpi(decltype(SPI)& spi = SPI) : spi_(spi) {}

  void init() { spi_.begin(); }

  void init(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_.begin(sck, miso, mosi);
  }

 private:
  template <typename SpiSettings>
  friend class ArduinoSpiDevice;

  decltype(SPI)& spi_;
};

// SPI transport, using the default <SPI.h> from the Arduino framework.
template <typename SpiSettings>
class ArduinoSpiDevice {
 public:
  // Creates the SPI transport, with specified transaction settings, using the
  // provided SPI bus.
  ArduinoSpiDevice(ArduinoSpi& spi) : spi_(spi.spi_) {}

  ArduinoSpiDevice() : spi_(SPI) {}

  void init() {}

  void beginReadWriteTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
  }

  void beginWriteOnlyTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
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

  void write32(uint32_t data) { spi_.write32(data); }

  // For whatever reasons, SPI.h doesn't have a const version of writeBytes, but
  // the data doesn't get mutated, so we can safely cast away constness here.
  void writeBytes_async(const roo::byte* data, uint32_t len) {
    auto* raw = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(data));
    spi_.writeBytes(raw, len);
  }

  void fill16_async(const roo::byte* data, uint32_t repetitions) {
    roo::byte buf[64];
    if (repetitions >= 32) {
      roo_io::PatternFill<2>(buf, 32, data);
      while (repetitions >= 32) {
        spi_.writeBytes(reinterpret_cast<uint8_t*>(buf), 64);
        repetitions -= 32;
      }
      spi_.writeBytes(reinterpret_cast<uint8_t*>(buf), repetitions * 2);
      return;
    }
    roo_io::PatternFill<2>(buf, repetitions, data);
    spi_.writeBytes(reinterpret_cast<uint8_t*>(buf), repetitions * 2);
  }

  void fill24_async(const roo::byte* data, uint32_t repetitions) {
    roo::byte buf[96];
    if (repetitions >= 32) {
      roo_io::PatternFill<3>(buf, 32, data);
      while (repetitions >= 32) {
        spi_.writeBytes(reinterpret_cast<uint8_t*>(buf), 96);
        repetitions -= 32;
      }
      spi_.writeBytes(reinterpret_cast<uint8_t*>(buf), repetitions * 3);
      return;
    }
    roo_io::PatternFill<3>(buf, repetitions, data);
    spi_.writeBytes(reinterpret_cast<uint8_t*>(buf), repetitions * 3);
  }

  roo::byte transfer(roo::byte data) {
    return static_cast<roo::byte>(spi_.transfer(static_cast<uint8_t>(data)));
  }
  uint16_t transfer16(uint16_t data) { return spi_.transfer16(data); }
  uint32_t transfer32(uint32_t data) { return spi_.transfer32(data); }

 private:
  decltype(SPI)& spi_;
};

}  // namespace roo_display

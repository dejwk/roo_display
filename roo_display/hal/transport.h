#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/internal/byte_order.h"
#include "roo_display/internal/memfill.h"

namespace roo_display {

// SPI transport, using the default <SPI.h> from the Arduino framework.
class GenericSpi {
 public:
  // Creates the SPI transport, with specified transaction settings, using the
  // provided SPI bus.
  GenericSpi(decltype(SPI)& spi, SPISettings settings)
      : spi_(spi), settings_(std::move(settings)) {}

  // Creates the SPI transport, with specified clock, and using MSBFIRST and
  // SPI_MODE0, using the provided SPI bus.
  GenericSpi(decltype(SPI)& spi, uint32_t clock)
      : spi_(spi), settings_(clock, MSBFIRST, SPI_MODE0) {}

  // Creates the SPI transport, with specified transaction settings, using the
  // default SPI bus.
  GenericSpi(SPISettings settings)
      : spi_(SPI), settings_(std::move(settings)) {}

  // Creates the SPI transport, with specified clock, and using MSBFIRST and
  // SPI_MODE0, using the default SPI bus.
  GenericSpi(uint32_t clock)
      : spi_(SPI), settings_(clock, MSBFIRST, SPI_MODE0) {}

  void beginTransaction() { spi_.beginTransaction(settings_); }
  void endTransaction() { spi_.endTransaction(); }

  void writeBytes(uint8_t* data, uint32_t len) { spi_.writeBytes(data, len); }

  void write(uint8_t data) { spi_.write(data); }

  void write16(uint16_t data) { spi_.write16(data); }

  void write16be(uint16_t data) { spi_.writeBytes((uint8_t*)&data, 2); }

  void write32(uint32_t data) { spi_.write32(data); }

  void write32be(uint32_t data) { spi_.writeBytes((uint8_t*)&data, 4); }

  void fill16(uint16_t data, uint32_t len) {
    fill16be(byte_order::htobe(data), len);
  }

  void fill16be(uint16_t data, uint32_t len) {
    uint8_t buf[64];
    if (len >= 32) {
      pattern_fill<2>(buf, 32, (uint8_t*)&data);
      while (len >= 32) {
        spi_.writeBytes(buf, 64);
        len -= 32;
      }
      spi_.writeBytes(buf, len * 2);
      return;
    }
    pattern_fill<2>(buf, len, (uint8_t*)&data);
    spi_.writeBytes(buf, len * 2);
  }

  uint8_t transfer(uint8_t data) { return spi_.transfer(data); }
  uint16_t transfer16(uint16_t data) { return spi_.transfer16(data); }
  uint32_t transfer32(uint32_t data) { return spi_.transfer32(data); }

 private:
  decltype(SPI)& spi_;
  SPISettings settings_;
};

class SpiTransactionConfig {
 public:
  SpiTransactionConfig(GenericSpi spi, int pin_cs)
      : spi_(spi), pin_cs_(pin_cs) {}

  GenericSpi& spi() { return spi_; }
  int pin_cs() const { return pin_cs_; }

 private:
  GenericSpi spi_;
  int pin_cs_;
};

class SpiTransaction {
 public:
  SpiTransaction(SpiTransactionConfig& config)
      : spi_(config.spi()), pin_cs_(config.pin_cs()) {
    spi_.beginTransaction();
    digitalWrite(pin_cs_, LOW);
  }
  ~SpiTransaction() {
    digitalWrite(pin_cs_, HIGH);
    spi_.endTransaction();
  }

 private:
  GenericSpi& spi_;
  int pin_cs_;
};

template <uint32_t _clock, uint8_t _bit_order, uint8_t _data_mode>
struct SpiSettings {
  static constexpr uint32_t clock = _clock;
  static constexpr uint8_t bit_order = _bit_order;
  static constexpr uint8_t data_mode = _data_mode;
};

// Convenience template wrapper that allows to capture all SPI configuration
// settings at compile time, inside a class specialization.
template <typename Spi, uint32_t clock, uint8_t bit_order, uint8_t data_mode>
class BoundSpi : public Spi {
 public:
  template <typename... Args>
  BoundSpi(Args&&... args)
      : Spi(std::forward<Args>(args)...,
            SPISettings(clock, bit_order, data_mode)) {}
};

// Partial specialization for GenericSpi.
template <uint32_t clock, uint8_t bit_order, uint8_t data_mode>
using BoundGenericSpi = BoundSpi<GenericSpi, clock, bit_order, data_mode>;

struct GenericSpiInterface {
  template <typename SpiSettings>
  using Transport = BoundGenericSpi<SpiSettings::clock, SpiSettings::bit_order,
                                    SpiSettings::data_mode>;
};

#ifdef ESP32

}  // namespace roo_display

#include "transport_esp32.h"

namespace roo_display {

typedef ::roo_display::esp32::Vspi DefaultSpiInterface;

#else

typedef GenericSpiInterface DefaultSpiInterface;

#endif

}  // namespace roo_display

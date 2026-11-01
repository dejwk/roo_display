#pragma once

#include <Arduino.h>

// Generic Arduino implementation.

#include <SPI.h>

#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/internal/byte_order.h"
#include "roo_io/data/byte_order.h"
#include "roo_io/memory/fill.h"
#include "roo_io/spi/spi.h"

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
#if defined(ARDUINO_ARCH_RP2040)
    if (!spi_.setSCK(sck) || !spi_.setRX(miso) || !spi_.setTX(mosi)) {
      panic("Invalid Arduino SPI pin assignment");
    }
    spi_.begin();
#else
    spi_.begin(sck, miso, mosi);
#endif
  }

  /// Borrows the initialized bus for synchronous peripheral drivers.
  /// Keep this bus initialized until every borrowing device is destroyed.
  roo_io::SpiMasterBusHandle busHandle() const {
    return roo_io::SpiMasterBusHandle(spi_);
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
    spi_.beginTransaction(
        SPISettings(SpiSettings::clock,
                    static_cast<decltype(MSBFIRST)>(SpiSettings::bit_order),
                    SpiSettings::data_mode));
  }

  void beginWriteOnlyTransaction() {
    spi_.beginTransaction(
        SPISettings(SpiSettings::clock,
                    static_cast<decltype(MSBFIRST)>(SpiSettings::bit_order),
                    SpiSettings::data_mode));
  }

  void endTransaction() { spi_.endTransaction(); }

  void flush() {}

  void write(uint8_t data) {
#if defined(ARDUINO_ARCH_RP2040)
    spi_.transfer(data);
#else
    spi_.write(data);
#endif
  }

  void write16(uint16_t data) {
#if defined(ARDUINO_ARCH_RP2040)
    spi_.transfer16(data);
#else
    spi_.write16(data);
#endif
  }

  void write16x2(uint16_t a, uint16_t b) {
    write16(a);
    write16(b);
  }

  // Pico provides a const transmit buffer API; ESP32's writeBytes also
  // preserves its input, but exposes it through a mutable pointer.
  void writeBytes(const roo::byte* data, uint32_t len) {
#if defined(ARDUINO_ARCH_RP2040)
    spi_.transfer(data, nullptr, len);
#else
    auto* raw = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(data));
    spi_.writeBytes(raw, len);
#endif
  }

  void fill16(const roo::byte* data, uint32_t repetitions) {
    roo::byte buf[64];
    if (repetitions >= 32) {
      roo_io::PatternFill<2>(buf, 32, data);
      while (repetitions >= 32) {
        writeBytes(buf, 64);
        repetitions -= 32;
      }
      writeBytes(buf, repetitions * 2);
      return;
    }
    roo_io::PatternFill<2>(buf, repetitions, data);
    writeBytes(buf, repetitions * 2);
  }

  void fill24(const roo::byte* data, uint32_t repetitions) {
    roo::byte buf[96];
    if (repetitions >= 32) {
      roo_io::PatternFill<3>(buf, 32, data);
      while (repetitions >= 32) {
        writeBytes(buf, 96);
        repetitions -= 32;
      }
      writeBytes(buf, repetitions * 3);
      return;
    }
    roo_io::PatternFill<3>(buf, repetitions, data);
    writeBytes(buf, repetitions * 3);
  }

  void fill16once(const roo::byte* data, uint32_t repetitions) {
    fill16(data, repetitions);
  }

  void fill24once(const roo::byte* data, uint32_t repetitions) {
    fill24(data, repetitions);
  }

  void asyncBlit(const roo::byte* data, size_t row_stride_bytes,
                 size_t row_bytes, size_t row_count) {
    if (data == nullptr || row_bytes == 0 || row_count == 0) {
      return;
    }

    if (row_stride_bytes == row_bytes) {
      writeBytes(data, static_cast<uint32_t>(row_bytes * row_count));
      return;
    }

    const roo::byte* row = data;
    for (size_t i = 0; i < row_count; ++i) {
      // Note that this is actually a synchronous call.
      writeBytes(row, static_cast<uint32_t>(row_bytes));
      row += row_stride_bytes;
    }
  }

  roo::byte transfer(roo::byte data) {
    return static_cast<roo::byte>(spi_.transfer(static_cast<uint8_t>(data)));
  }
  uint16_t transfer16(uint16_t data) { return spi_.transfer16(data); }

 private:
  decltype(SPI)& spi_;
};

}  // namespace roo_display

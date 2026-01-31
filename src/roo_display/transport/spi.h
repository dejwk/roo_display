#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/hal/gpio.h"
#include "roo_display/hal/spi.h"
#include "roo_display/hal/spi_settings.h"
#include "roo_display/internal/byte_order.h"

namespace roo_display {

template <int pinCS, typename SpiDevice, typename Gpio>
class SpiReadWriteTransaction {
 public:
  SpiReadWriteTransaction(SpiDevice& device) : device_(device) {
    device_.beginReadWriteTransaction();
    Gpio::template setLow<pinCS>();
  }
  ~SpiReadWriteTransaction() {
    Gpio::template setHigh<pinCS>();
    device_.endTransaction();
  }

 private:
  SpiDevice& device_;
};

// Convenience helper class for device drivers that use 'chip select',
// 'data/command', and optionally, 'reset' pins to control the underlying
// transport such as SPI.
// If pinRST is negative, it is ignored. Otherwise, it is set to HIGH.
// If pinDC is negative, the methods cmdBegin() and cmdEnd() should not be
// called.
//
// NOTE(dawidk): force-inlining the methods doesn't seem to do anything; they
// are inlined already.
template <int pinCS, int pinDC, int pinRST, typename SpiSettings,
          typename Spi = DefaultSpi, typename Gpio = DefaultGpio>
class SpiTransport {
 public:
  template <typename... Args>
  SpiTransport(Args&&... args)
      : spi_(std::forward<Args>(args)...), device_(spi_) {
    Gpio::setOutput(pinCS);
    cs_h();

    if (pinDC >= 0) {
      Gpio::setOutput(pinDC);
      dc_d();
    }

    if (pinRST >= 0) {
      Gpio::setOutput(pinRST);
      // Make sure that RESET is unclicked
      Gpio::template setHigh<pinRST>();
    }
  }

  void begin() { cs_l(); }

  void end() { cs_h(); }

  void cmdBegin() { dc_c(); }

  void cmdEnd() { dc_d(); }

  void init() { device_.init(); }

  void beginReadWriteTransaction() { device_.beginReadWriteTransaction(); }

  void beginWriteOnlyTransaction() { device_.beginWriteOnlyTransaction(); }

  void endTransaction() { device_.endTransaction(); }

  void sync() __attribute__((always_inline)) { device_.sync(); }

  void write(uint8_t data) __attribute__((always_inline)) {
    device_.write(data);
  }

  void write16(uint16_t data) __attribute__((always_inline)) {
    device_.write16(data);
  }

  void write16x2(uint16_t a, uint16_t b) __attribute((always_inline)) {
    device_.write16x2(a, b);
  }

  void write16x2_async(uint16_t a, uint16_t b) __attribute((always_inline)) {
    device_.write16x2_async(a, b);
  }

  void write16be(uint16_t data) __attribute((always_inline)) {
    device_.write16be(data);
  }

  void write32(uint32_t data) __attribute__((always_inline)) {
    device_.write32(data);
  }

  void write32be(uint32_t data) __attribute__((always_inline)) {
    device_.write32be(data);
  }

  void writeBytes_async(uint8_t* data, uint32_t len) {
    device_.writeBytes_async(data, len);
  }

  void fill16_async(uint16_t data, uint32_t len)
      __attribute__((always_inline)) {
    device_.fill16be_async(data, len);
  }

  void fill16be_async(uint16_t data, uint32_t len) {
    device_.fill16be_async(data, len);
  }

  void fill24be_async(uint32_t data, uint32_t len) {
    device_.fill24be_async(data, len);
  }

  uint8_t transfer(uint8_t data) __attribute__((always_inline)) {
    return device_.transfer(data);
  }

  uint16_t transfer16(uint16_t data) __attribute__((always_inline)) {
    return device_.transfer16(data);
  }

  uint32_t transfer32(uint32_t data) __attribute__((always_inline)) {
    return device_.transfer32(data);
  }

 private:
  Spi spi_;
  typename Spi::Device<SpiSettings> device_;

  void dc_c() { Gpio::template setLow<pinDC>(); }
  void dc_d() { Gpio::template setHigh<pinDC>(); }
  void cs_l() { Gpio::template setLow<pinCS>(); }
  void cs_h() { Gpio::template setHigh<pinCS>(); }
};

}  // namespace roo_display

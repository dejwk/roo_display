#pragma once

#include "roo_backport.h"
#include "roo_backport/byte.h"
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

  void begin() __attribute__((always_inline)) { cs_l(); }

  void end() __attribute__((always_inline)) {
    device_.flush();
    cs_h();
  }

  void cmdBegin() __attribute__((always_inline)) {
    device_.flush();
    dc_c();
  }

  void cmdEnd() __attribute__((always_inline)) {
    device_.flush();
    dc_d();
  }

  void init() { device_.init(); }

  void beginReadWriteTransaction() { device_.beginReadWriteTransaction(); }

  void beginWriteOnlyTransaction() { device_.beginWriteOnlyTransaction(); }

  void endTransaction() { device_.endTransaction(); }

  void flush() __attribute__((always_inline)) { device_.flush(); }

  void write(uint8_t data) __attribute__((always_inline)) {
    device_.write(data);
  }

  void write16(uint16_t data) __attribute__((always_inline)) {
    device_.write16(data);
  }

  void write16x2(uint16_t a, uint16_t b) __attribute__((always_inline)) {
    device_.write16x2(a, b);
  }

  void writeBytes(const roo::byte* data, uint32_t len) {
    device_.writeBytes(data, len);
  }

  void fill16(const roo::byte* data, uint32_t repetitions) {
    device_.fill16(data, repetitions);
  }

  void fill24(const roo::byte* data, uint32_t repetitions) {
    device_.fill24(data, repetitions);
  }

  void async_blit(const roo::byte* data, size_t row_stride_bytes,
                  size_t row_bytes, size_t row_count) {
    device_.async_blit(data, row_stride_bytes, row_bytes, row_count);
  }

  roo::byte transfer(roo::byte data) __attribute__((always_inline)) {
    return device_.transfer(data);
  }

  uint16_t transfer16(uint16_t data) __attribute__((always_inline)) {
    return device_.transfer16(data);
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

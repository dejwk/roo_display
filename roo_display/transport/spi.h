#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/hal/gpio.h"
#include "roo_display/hal/spi.h"
#include "roo_display/internal/byte_order.h"
#include "roo_display/internal/memfill.h"

namespace roo_display {

template <uint32_t _clock, uint8_t _bit_order, uint8_t _data_mode>
struct SpiSettings {
  static constexpr uint32_t clock = _clock;
  static constexpr uint8_t bit_order = _bit_order;
  static constexpr uint8_t data_mode = _data_mode;
};

// Convenience template wrapper that allows to capture all SPI configuration
// settings at compile time, inside a class specialization.
template <typename Spi, typename SpiSettings>
class BoundSpi : public Spi {
 public:
  template <typename... Args>
  BoundSpi(Args&&... args) : Spi(std::forward<Args>(args)...) {}

  void beginTransaction() {
    SPISettings settings(SpiSettings::clock, SpiSettings::bit_order,
                         SpiSettings::data_mode);
    Spi::beginTransaction(settings);
  }
};

template <int pinCS, typename BoundSpi, typename Gpio>
class BoundSpiTransaction {
 public:
  BoundSpiTransaction(BoundSpi& spi) : spi_(spi) {
    spi_.beginTransaction();
    Gpio::template setLow<pinCS>();
  }
  ~BoundSpiTransaction() {
    Gpio::template setHigh<pinCS>();
    spi_.endTransaction();
  }

 private:
  BoundSpi& spi_;
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
class SpiTransport : public BoundSpi<Spi, SpiSettings> {
 public:
  template <typename... Args>
  SpiTransport(Args&&... args)
      : BoundSpi<Spi, SpiSettings>(std::forward<Args>(args)...) {
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

 private:
  void dc_c() { Gpio::template setLow<pinDC>(); }
  void dc_d() { Gpio::template setHigh<pinDC>(); }
  void cs_l() { Gpio::template setLow<pinCS>(); }
  void cs_h() { Gpio::template setHigh<pinCS>(); }
};

}  // namespace roo_display

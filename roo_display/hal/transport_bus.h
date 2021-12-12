#pragma once

#include "roo_display/hal/gpio.h"
#include "roo_display/hal/transport.h"

namespace roo_display {

// Convenience helper class for device drivers that use 'chip select',
// 'data/command', and optionally, 'reset' pins to control the underlying
// transport such as SPI.
// If pinRST is negative, it is ignored. Otherwise, it is set to HIGH.
// If pinDC is negative, the methods cmdBegin() and cmdEnd() should not be
// called.
template <int pinCS, int pinDC, int pinRST, typename Gpio = DefaultGpio>
class TransportBus {
 public:
  TransportBus() {
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

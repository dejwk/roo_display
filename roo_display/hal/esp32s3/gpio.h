#pragma once

#include "soc/gpio_struct.h"

namespace roo_display {
namespace esp32s3 {

struct Gpio {
  static void setOutput(int pin) { pinMode(pin, OUTPUT); }

  template <int pin>
  static void setLow() {
    if (pin < 32) {
      *(PORTreg_t)&GPIO.out_w1tc = digitalPinToBitMask((int8_t)pin);
    } else {
      *(PORTreg_t)&GPIO.out1_w1tc.val = digitalPinToBitMask((int8_t)pin);
    }
  }

  template <int pin>
  static void setHigh() {
    if (pin < 32) {
      *(PORTreg_t)&GPIO.out_w1ts = digitalPinToBitMask((int8_t)pin);
    } else {
      *(PORTreg_t)&GPIO.out1_w1ts.val = digitalPinToBitMask((int8_t)pin);
    }
  }
};

}  // namespace esp32s3

using DefaultGpio = esp32s3::Gpio;

}  // namespace roo_display

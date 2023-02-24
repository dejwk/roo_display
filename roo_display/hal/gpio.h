#pragma once

#include <Arduino.h>

#if (defined(ESP32) && !defined(ROO_TESTING))

#include "soc/gpio_struct.h"
#endif

namespace roo_display {

#if (defined(ESP32) && !defined(ROO_TESTING))

struct Esp32Gpio {
  static void setOutput(int pin) { pinMode(pin, OUTPUT); }

  template <int pin>
  static void setLow() {
    if (pin < 32) {
      GPIO.out_w1ts = (1 << pin);
      GPIO.out_w1tc = (1 << pin);
    } else {
      GPIO.out1_w1tc.val = (1 << (pin - 32));
    }
  }

  template <int pin>
  static void setHigh() {
    if (pin < 32) {
      GPIO.out_w1tc = (1 << pin);
      GPIO.out_w1ts = (1 << pin);
    } else {
      GPIO.out1_w1ts.val = (1 << (pin - 32));
    }
  }
};

typedef Esp32Gpio DefaultGpio;

#else

struct ArduinoGpio {
  static void setOutput(int pin) { pinMode(pin, OUTPUT); }

  template <int pin>
  static void setLow() {
    digitalWrite(pin, LOW);
  }

  template <int pin>
  static void setHigh() {
    digitalWrite(pin, HIGH);
  }
};

typedef ArduinoGpio DefaultGpio;

#endif

}  // namespace roo_display

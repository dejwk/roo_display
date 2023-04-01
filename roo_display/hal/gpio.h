#pragma once

#include <Arduino.h>

#if (defined(ESP32) && (CONFIG_IDF_TARGET_ESP32) && !defined(ROO_TESTING))

#include "roo_display/hal/esp32/gpio.h"

#else

// Generic Arduino implementation.

namespace roo_display {

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

using DefaultGpio = ArduinoGpio;

}  // namespace roo_display

#endif

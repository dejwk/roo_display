#pragma once

#include <Arduino.h>

#if (defined(ESP32) && (CONFIG_IDF_TARGET_ESP32) && !defined(ROO_TESTING))

#include "roo_display/hal/esp32/gpio.h"

#elif (defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3) && !defined(ROO_TESTING))

#include "roo_display/hal/esp32s3/gpio.h"

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

  // Non-templated versions as well, for dynamic pin numbers.
  static void setLow(int pin) { digitalWrite(pin, LOW); }
  static void setHigh(int pin) { digitalWrite(pin, HIGH); }
};

using DefaultGpio = ArduinoGpio;

}  // namespace roo_display

#endif

#pragma once

#include "roo_display/hal/config.h"

#if (defined(ESP_PLATFORM) && !defined(ROO_TESTING))

#if (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2)
#include "roo_display/hal/esp32/gpio.h"
#elif (CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || \
       CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C6 || \
       CONFIG_IDF_TARGET_ESP32H2)
#include "roo_display/hal/esp32s3/gpio.h"
#else
#error "Unsupported ESP32 variant"
#endif

#elif defined(ARDUINO)

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

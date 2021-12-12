#pragma once

#include <Arduino.h>

namespace roo_display {

#if defined(ESP32)

struct Esp32Gpio {
  static void setOutput(int pin) { pinMode(pin, OUTPUT); }

  template <int pin>
  static void setLow() {
    GPIO.out_w1ts = (1 << pin);
    GPIO.out_w1tc = (1 << pin);
  }

  template <int pin>
  static void setHigh() {
    GPIO.out_w1tc = (1 << pin);
    GPIO.out_w1ts = (1 << pin);
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

#pragma once

#include "esp32-hal.h"
#include "roo_display/backlit/backlit.h"

namespace roo_display {

class LedcBacklit : public Backlit {
 public:
  LedcBacklit(int pin, uint8_t intensity = 255)
      : LedcBacklit(pin, 0, intensity) {}

#if (defined ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
 private:
#else
 public:
#endif
  LedcBacklit(int pin, int channel, uint8_t intensity = 255)
      : pin_(pin), channel_(channel) {
    pinMode(pin, OUTPUT);

#if (defined ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(pin_, 50000, 8);
#else
    ledcSetup(channel_, 50000, 8);
    ledcAttachPin(pin_, channel_);
#endif
    ledcWrite(channel_, intensity);
  }

 public:
  void setIntensity(uint8_t intensity) override {
    ledcWrite(channel_, intensity);
  }

 private:
  int pin_;
  int channel_;
};

};  // namespace roo_display

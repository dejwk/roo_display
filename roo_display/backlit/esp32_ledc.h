#pragma once

#include "esp32-hal.h"
#include "roo_display/backlit/backlit.h"

namespace roo_display {

class LedcBacklit : public Backlit {
 public:
  LedcBacklit(int pin, int channel, uint8_t intensity = 255)
      : pin_(pin), channel_(channel) {
    pinMode(pin, OUTPUT);
    ledcSetup(channel_, 50000, 8);
    ledcAttachPin(pin_, channel_);
    ledcWrite(channel_, intensity);
  }

  void setIntensity(uint8_t intensity) override {
    ledcWrite(channel_, intensity);
  }

 private:
  int pin_;
  int channel_;
};

};  // namespace roo_display

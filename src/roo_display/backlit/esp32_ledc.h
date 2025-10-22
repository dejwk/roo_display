#pragma once

#include "esp32-hal.h"
#include "roo_display/backlit/backlit.h"

namespace roo_display {

#if (defined ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)

class LedcBacklit : public Backlit {
 public:
  LedcBacklit(uint8_t pin) : pin_(pin) {}

 public:
  void begin(uint16_t intensity = kMaxIntensity) {
    pinMode(pin_, OUTPUT);
    // Use 50 kHz frequency to avoid interference with audio.
    CHECK(ledcAttach(pin_, 50000, 8));
    CHECK(ledcWrite(pin_, intensity));
  }

  void setIntensity(uint16_t intensity) override {
    CHECK(ledcWrite(pin_, intensity));
  }

 private:
  uint8_t pin_;
};

class LedcBacklitAtChannel : public Backlit {
 public:
  LedcBacklitAtChannel(uint8_t pin, uint8_t channel)
      : pin_(pin), channel_(channel) {}

 public:
  void begin(uint16_t intensity = kMaxIntensity) {
    pinMode(pin_, OUTPUT);
    // Use 50 kHz frequency to avoid interference with audio.
    CHECK(ledcAttachChannel(pin_, 50000, 8, channel_));
    CHECK(ledcWriteChannel(channel_, intensity));
  }

  void setIntensity(uint16_t intensity) override {
    CHECK(ledcWriteChannel(channel_, intensity));
  }

 private:
  uint8_t pin_;
  int8_t channel_;
};

#else

// With Arduino < 3, the channel must be specified explicitly.

class LedcBacklitAtChannel : public Backlit {
 public:
  LedcBacklitAtChannel(uint8_t pin, uint8_t channel)
      : pin_(pin), channel_(channel) {}

 public:
  void begin(uint16_t intensity = kMaxIntensity) {
    pinMode(pin_, OUTPUT);
    // Use 50 kHz frequency to avoid interference with audio.
    ledcSetup(channel_, 50000, 8);
    ledcAttachPin(pin_, channel_);
    ledcWrite(channel_, intensity);
  }

  void setIntensity(uint16_t intensity) override {
    ledcWrite(channel_, intensity);
  }

 private:
  uint8_t pin_;
  uint8_t channel_;
};

#endif

};  // namespace roo_display

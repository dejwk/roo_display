#pragma once

#include "roo_display/backlit/backlit.h"
#include "roo_display/hal/gpio.h"

#if defined(ESP_PLATFORM)
#include "driver/ledc.h"
#include "esp_err.h"

namespace roo_display {

#if defined(ARDUINO)
#if (defined ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)

class LedcBacklit : public Backlit {
 public:
  LedcBacklit(uint8_t pin) : pin_(pin) {}

 public:
  void begin(uint16_t intensity = kMaxIntensity) {
    DefaultGpio::setOutput(pin_);
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
    DefaultGpio::setOutput(pin_);
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
    DefaultGpio::setOutput(pin_);
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
#else  // defined(ARDUINO)

// Note: always uses LEDC_CHANNEL_0 and LEDC_TIMER_0. Use LedcBacklitAtChannel
// to specify a different channel or timer.
class LedcBacklit : public Backlit {
 public:
  explicit LedcBacklit(uint8_t pin)
      : pin_(pin), channel_(LEDC_CHANNEL_0), timer_(LEDC_TIMER_0) {}

  void begin(uint16_t intensity = kMaxIntensity) {
    DefaultGpio::setOutput(pin_);
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = timer_,
        .freq_hz = 50000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .gpio_num = pin_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    setIntensity(intensity);
  }

  void setIntensity(uint16_t intensity) override {
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, intensity));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_));
  }

 private:
  uint8_t pin_;
  ledc_channel_t channel_;
  ledc_timer_t timer_;
};

class LedcBacklitAtChannel : public Backlit {
 public:
  LedcBacklitAtChannel(uint8_t pin, uint8_t channel)
      : pin_(pin),
        channel_(static_cast<ledc_channel_t>(channel)),
        timer_(LEDC_TIMER_0) {}

  // ESP-IDF style constructor allowing timer selection.
  LedcBacklitAtChannel(uint8_t pin, ledc_channel_t channel,
                       ledc_timer_t timer = LEDC_TIMER_0)
      : pin_(pin),
        channel_(static_cast<ledc_channel_t>(channel)),
        timer_(timer) {}

  void begin(uint16_t intensity = kMaxIntensity) {
    DefaultGpio::setOutput(pin_);
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = timer_,
        .freq_hz = 50000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    ledc_channel_config_t channel_config = {
        .gpio_num = pin_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    setIntensity(intensity);
  }

  void setIntensity(uint16_t intensity) override {
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, intensity));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_));
  }

 private:
  uint8_t pin_;
  ledc_channel_t channel_;
  ledc_timer_t timer_;
};

#endif  // defined(ARDUINO)

}  // namespace roo_display

#endif  // defined(ESP_PLATFORM)

#pragma once

#include "driver/gpio.h"
#include "roo_display/hal/config.h"
#include "soc/gpio_struct.h"

#ifdef ARDUINO
// Support possible pin remapping in Arduino framework.
#include "Arduino.h"
#define ROO_DISPLAY_GPIO_PIN_REMAP(pin) digitalPinToGPIONumber(pin)
#else
#define ROO_DISPLAY_GPIO_PIN_REMAP(pin) (pin)
#endif

// ESP32S3 uses direct register access, while C3/C2/C6/H2 use .val
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
#define GPIO_OUT_W1T_CLR(value) (GPIO.out_w1tc.val = (value))
#define GPIO_OUT_W1T_SET(value) (GPIO.out_w1ts.val = (value))
#else
#define GPIO_OUT_W1T_CLR(value) (GPIO.out_w1tc = (value))
#define GPIO_OUT_W1T_SET(value) (GPIO.out_w1ts = (value))
#endif

namespace roo_display {
namespace esp32s3 {

struct Gpio {
  static void setOutput(int pin) {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    gpio_reset_pin((gpio_num_t)gpio);
    gpio_set_direction((gpio_num_t)gpio, GPIO_MODE_OUTPUT);
  }

  // Templated setLow will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setLow() {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO_OUT_W1T_CLR(1UL << gpio);
#if CONFIG_IDF_TARGET_ESP32S3
    } else {
      GPIO.out1_w1tc.val = (1UL << (gpio - 32));
#endif
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setHigh() {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO_OUT_W1T_SET(1UL << gpio);
#if CONFIG_IDF_TARGET_ESP32S3
    } else {
      GPIO.out1_w1ts.val = (1UL << (gpio - 32));
#endif
    }
  }

  // Non-templated versions as well, for when pin numbers are not fixed at
  // compile time.
  static void setLow(int pin) {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO_OUT_W1T_CLR(1UL << gpio);
#if CONFIG_IDF_TARGET_ESP32S3
    } else {
      GPIO.out1_w1tc.val = (1UL << (gpio - 32));
#endif
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  static void setHigh(int pin) {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO_OUT_W1T_SET(1UL << gpio);
#if CONFIG_IDF_TARGET_ESP32S3
    } else {
      GPIO.out1_w1ts.val = (1UL << (gpio - 32));
#endif
    }
  }
};

}  // namespace esp32s3

using DefaultGpio = esp32s3::Gpio;

}  // namespace roo_display

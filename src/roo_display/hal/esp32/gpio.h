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

// Variant-specific register access and output bank support.
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || \
    CONFIG_IDF_TARGET_ESP32S3

#define ROO_DISPLAY_GPIO_ESP32_SET(pin)                \
  do {                                                 \
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin); \
    if (gpio < 32) {                                   \
      GPIO.out_w1ts = (1UL << gpio);                   \
    } else {                                           \
      GPIO.out1_w1ts.val = (1UL << (gpio - 32));       \
    }                                                  \
  } while (0)

#define ROO_DISPLAY_GPIO_ESP32_CLR(pin)                \
  do {                                                 \
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin); \
    if (gpio < 32) {                                   \
      GPIO.out_w1tc = (1UL << gpio);                   \
    } else {                                           \
      GPIO.out1_w1tc.val = (1UL << (gpio - 32));       \
    }                                                  \
  } while (0)

#elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2

#define ROO_DISPLAY_GPIO_ESP32_SET(pin)                \
  do {                                                 \
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin); \
    GPIO.out_w1ts.val = (1UL << gpio);                 \
  } while (0)

#define ROO_DISPLAY_GPIO_ESP32_CLR(pin)                \
  do {                                                 \
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin); \
    GPIO.out_w1tc.val = (1UL << gpio);                 \
  } while (0)

#else
#error "Unsupported ESP32 variant"
#endif

namespace roo_display {
namespace esp32 {

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
    ROO_DISPLAY_GPIO_ESP32_CLR(pin);
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setHigh() {
    ROO_DISPLAY_GPIO_ESP32_SET(pin);
  }

  // Non-templated versions as well, for when pin numbers are not fixed at
  // compile time.
  static void setLow(int pin) { ROO_DISPLAY_GPIO_ESP32_CLR(pin); }
  static void setHigh(int pin) { ROO_DISPLAY_GPIO_ESP32_SET(pin); }
};

}  // namespace esp32

using DefaultGpio = esp32::Gpio;

}  // namespace roo_display

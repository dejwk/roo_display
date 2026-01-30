#pragma once

#include "driver/gpio.h"
#include "soc/gpio_struct.h"

#ifdef ARDUINO
// Support possible pin remapping in Arduino framework.
#include "Arduino.h"
#define ROO_DISPLAY_GPIO_PIN_REMAP(pin) digitalPinToGPIONumber(pin)
#else
#define ROO_DISPLAY_GPIO_PIN_REMAP(pin) (pin)
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
      GPIO.out_w1tc = (1UL << gpio);
    } else {
      GPIO.out1_w1tc.val = (1UL << (gpio - 32));
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setHigh() {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO.out_w1ts = (1UL << gpio);
    } else {
      GPIO.out1_w1ts.val = (1UL << (gpio - 32));
    }
  }

  // Non-templated versions as well, for when pin numbers are not fixed at
  // compile time.
  static void setLow(int pin) {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO.out_w1tc = (1UL << gpio);
    } else {
      GPIO.out1_w1tc.val = (1UL << (gpio - 32));
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  static void setHigh(int pin) {
    const auto gpio = ROO_DISPLAY_GPIO_PIN_REMAP(pin);
    if (gpio < 32) {
      GPIO.out_w1ts = (1UL << gpio);
    } else {
      GPIO.out1_w1ts.val = (1UL << (gpio - 32));
    }
  }
};

}  // namespace esp32s3

using DefaultGpio = esp32s3::Gpio;

}  // namespace roo_display

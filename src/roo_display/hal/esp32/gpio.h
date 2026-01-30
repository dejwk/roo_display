#pragma once

#include "driver/gpio.h"
#include "soc/gpio_struct.h"

namespace roo_display {
namespace esp32 {

struct Gpio {
  static void setOutput(int pin) {
    gpio_reset_pin((gpio_num_t)pin);
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  }

  // Templated setLow will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setLow() {
    if (pin < 32) {
      GPIO.out_w1tc = (1 << pin);
    } else {
      GPIO.out1_w1tc.val = (1 << (pin - 32));
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setHigh() {
    if (pin < 32) {
      GPIO.out_w1ts = (1 << pin);
    } else {
      GPIO.out1_w1ts.val = (1 << (pin - 32));
    }
  }

  // Non-templated versions as well, for when pin numbers are not fixed at
  // compile time.
  static void setLow(int pin) {
    if (pin < 32) {
      GPIO.out_w1tc = (1 << pin);
    } else {
      GPIO.out1_w1tc.val = (1 << (pin - 32));
    }
  }

  static void setHigh(int pin) {
    if (pin < 32) {
      GPIO.out_w1ts = (1 << pin);
    } else {
      GPIO.out1_w1ts.val = (1 << (pin - 32));
    }
  }
};

}  // namespace esp32

using DefaultGpio = esp32::Gpio;

}  // namespace roo_display

#pragma once

#include "driver/gpio.h"
#include "soc/gpio_struct.h"

namespace roo_display {
namespace esp32s3 {

struct Gpio {
  static void setOutput(int pin) {
    gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  }

  // Templated setLow will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setLow() {
    if (pin < 32) {
      GPIO.out_w1tc = digitalPinToBitMask((int8_t)pin);
    } else {
      GPIO.out1_w1tc.val = digitalPinToBitMask((int8_t)pin);
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  template <int pin>
  static void setHigh() {
    if (pin < 32) {
      GPIO.out_w1ts = digitalPinToBitMask((int8_t)pin);
    } else {
      GPIO.out1_w1ts.val = digitalPinToBitMask((int8_t)pin);
    }
  }

  // Non-templated versions as well, for when pin numbers are not fixed at
  // compile time.
  static void setLow(int pin) {
    if (pin < 32) {
      GPIO.out_w1tc = digitalPinToBitMask((int8_t)pin);
    } else {
      GPIO.out1_w1tc.val = digitalPinToBitMask((int8_t)pin);
    }
  }

  // Templated setHigh will be inlined to a single register write with a
  // constant.
  static void setHigh(int pin) {
    if (pin < 32) {
      GPIO.out_w1ts = digitalPinToBitMask((int8_t)pin);
    } else {
      GPIO.out1_w1ts.val = digitalPinToBitMask((int8_t)pin);
    }
  }
};

}  // namespace esp32s3

using DefaultGpio = esp32s3::Gpio;

}  // namespace roo_display

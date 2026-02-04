#pragma once

#include <cstdint>
#include <functional>

#include "roo_display/hal/gpio.h"

namespace roo_display {

// An abstraction for non-performance-sensitive GPIO signals that might be wired
// through port extenders, rather than direct MCU pins.
class GpioSetter {
 public:
  // Gets called to initialize the pin for output.
  using InitFn = std::function<void()>;

  // Gets called with state = 0 to set low, and state = 1 to set high.
  using SetterFn = std::function<void(uint8_t state)>;

  // Creates an inatcive setter (setHigh and setLow are no-ops).
  GpioSetter() : setter_(), init_() {}

  // Implicit conversion from pin number, using DefaultGpio.
  GpioSetter(int8_t pin) : setter_(nullptr), init_(nullptr) {
    if (pin >= 0) {
      setter_ = [pin](uint8_t state) {
        if (state > 0) {
          DefaultGpio::setHigh(pin);
        } else {
          DefaultGpio::setLow(pin);
        }
      };
      init_ = [pin]() { DefaultGpio::setOutput(pin); };
    }
  }

  // Implicit conversion from a lambda setter function, in case no
  // initialization is needed.
  GpioSetter(SetterFn setter) : setter_(std::move(setter)), init_() {}

  // Explicit constructor in cases when initialization is needed.
  GpioSetter(SetterFn setter, InitFn init)
      : setter_(std::move(setter)), init_(std::move(init)) {}

  bool isDefined() const { return setter_ != nullptr; }

  void init() {
    if (init_ != nullptr) {
      init_();
    }
  }

  void setHigh() {
    if (setter_ != nullptr) setter_(1);
  }

  void setLow() {
    if (setter_ != nullptr) setter_(0);
  }

 private:
  SetterFn setter_;
  InitFn init_;
};

}  // namespace roo_display

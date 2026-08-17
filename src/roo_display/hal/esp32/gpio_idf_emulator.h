#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

namespace roo_display {
namespace esp32 {

// ESP-IDF GPIO transport used by roo_testing. The production transport writes
// peripheral registers directly, which the host emulator deliberately does not
// model for the IDF-only frontend. Use the public driver API so writes reach
// the existing GPIO shim.
struct IdfEmulatorGpio {
  static void setOutput(int pin) {
    const auto gpio = static_cast<gpio_num_t>(pin);
    ESP_ERROR_CHECK(gpio_reset_pin(gpio));
    ESP_ERROR_CHECK(gpio_set_direction(gpio, GPIO_MODE_OUTPUT));
  }

  template <int pin>
  static void setLow() {
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 0));
  }

  template <int pin>
  static void setHigh() {
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 1));
  }

  static void setLow(int pin) {
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 0));
  }

  static void setHigh(int pin) {
    ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pin), 1));
  }
};

}  // namespace esp32
}  // namespace roo_display

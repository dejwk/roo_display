#pragma once

// https://www.makerfabs.com/esp32-3-5-inch-tft-touch-capacitive-with-camera.html
// Maker: MakerFabs
// Product Code: ESPTFT35CA

#include "roo_display/hal/config.h"
#if !defined(ESP_PLATFORM) || !CONFIG_IDF_TARGET_ESP32
#warning "This driver requires ESP32 SoC."
#else

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/driver/touch_ft6x36.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/hal/spi.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::makerfabs {

class Esp32TftCapacitive35 : public ComboDevice {
 public:
#if defined(ARDUINO)
  // You can use non-default I2C interface by providing a custom instance of
  // TwoWire as the i2c parameter.
  Esp32TftCapacitive35(
      Orientation orientation = Orientation(),
      roo_display::I2cMasterBusHandle i2c = roo_display::I2cMasterBusHandle())
      : hspi_(HSPI), spi_(hspi_), i2c_(i2c), display_(spi_), touch_(i2c_) {
    display_.setOrientation(orientation);
  }
#else
  // You can use non-default I2C interface by providing a specific
  // i2c_port_num_t as the i2c parameter.
  Esp32TftCapacitive35(
      Orientation orientation = Orientation(),
      roo_display::I2cMasterBusHandle i2c = roo_display::I2cMasterBusHandle())
      : spi_(), i2c_(i2c), display_(spi_), touch_(i2c_) {
    display_.setOrientation(orientation);
  }
#endif

  void initTransport() {
    spi_.init(14, 12, 13);
    i2c_.init(26, 27);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  TouchCalibration touch_calibration() override {
    return TouchCalibration(0, 20, 309, 454, Orientation::RightDown());
  }

  constexpr int8_t pin_sck() const { return 14; }
  constexpr int8_t pin_miso() const { return 12; }
  constexpr int8_t pin_mosi() const { return 13; }
  constexpr int8_t pin_sda() const { return 26; }
  constexpr int8_t pin_scl() const { return 27; }

  constexpr int8_t pin_sd_cs() const { return 4; }

#if defined(ARDUINO)
  decltype(SPI)& spi() { return hspi_; }
#endif

 private:
#if defined(ARDUINO)
  decltype(SPI) hspi_;
#endif

  roo_display::esp32::Hspi spi_;
  roo_display::I2cMasterBusHandle i2c_;
  roo_display::Ili9488spi<15, 33, 26, esp32::Hspi> display_;
  roo_display::TouchFt6x36 touch_;
};

}  // namespace roo_display::products::makerfabs

#endif  // !defined(ESP_PLATFORM) || !CONFIG_IDF_TARGET_ESP32

#pragma once

// https://www.makerfabs.com/esp32-3-5-inch-tft-touch-capacitive-with-camera.html
// Maker: MakerFabs
// Product Code: ESPTFT35CA

#include <SPI.h>
#include <Wire.h>

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/driver/touch_ft6x36.h"
#include "roo_display/hal/spi.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::makerfabs {

class Esp32TftCapacitive35 : public ComboDevice {
 public:
  Esp32TftCapacitive35(Orientation orientation = Orientation(),
                       decltype(Wire)& wire = Wire)
      : spi_(HSPI), wire_(wire), display_(spi_), touch_() {
    display_.setOrientation(orientation);
  }

  void initTransport() {
    spi_.begin(14, 12, 13);
    wire_.begin(26, 27);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  TouchCalibration touch_calibration() override {
    return TouchCalibration(0, 20, 309, 454, Orientation::RightDown());
  }

  decltype(SPI)& spi() { return spi_; }

  constexpr int8_t pin_sck() const { return 14; }
  constexpr int8_t pin_miso() const { return 12; }
  constexpr int8_t pin_mosi() const { return 13; }
  constexpr int8_t pin_sda() const { return 26; }
  constexpr int8_t pin_scl() const { return 27; }

  constexpr int8_t pin_sd_cs() const { return 4; }

 private:
  decltype(SPI) spi_;
  decltype(Wire)& wire_;
  roo_display::Ili9488spi<15, 33, 26, esp32::Hspi> display_;
  roo_display::TouchFt6x36 touch_;
};

}  // namespace roo_display::products::makerfabs

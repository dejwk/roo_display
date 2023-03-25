#pragma once

// https://www.makerfabs.com/esp32-3.5-inch-tft-touch-capacitive-with-camera.html
// Maker: MakerFabs
// Product Code: ESPTFT35CA

#include <SPI.h>
#include <Wire.h>

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/driver/touch_ft6x36.h"
#include "roo_display/hal/transport.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::makerfabs {

class Esp32TftCapacitive35 : public ComboDevice {
 public:
  Esp32TftCapacitive35(Orientation orientation = Orientation(),
                       decltype(SPI)& spi = SPI, decltype(Wire)& wire = Wire)
      : spi_(spi), wire_(wire), display_(spi), touch_() {
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

 private:
  decltype(SPI)& spi_;
  decltype(Wire)& wire_;
  roo_display::Ili9488spi<15, 33, 26> display_;
  roo_display::TouchFt6x36 touch_;
};

}  // namespace roo_display::products::makerfabs

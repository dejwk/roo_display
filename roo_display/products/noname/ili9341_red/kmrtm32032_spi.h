#pragma once

// https://http://www.lcdwiki.com/3.2inch_SPI_Module_ILI9341_SKU:MSP3218
// Maker: ???
// SKU: MSP3218 ?
//
// Note: if you want to use the built-in SD card reader, treat it as a
// completely separate SPI device, and use the appropriate libraries (e.g.,
// 'SD'). You can connect the SD reader to the same SPI bus as the display. You
// then just need to ensure that its 'CS' pin is never activated (set to LOW)
// when a display's DrawingContext exists.
//
// Note: this display has a backlit (LED) pin. You can connect it to VCC for
// full brigtness. You can also connect it to some digital output pin, and use
// the controller-specific utility (e.g. LedcBacklit on ESP32) to control the
// intensity.

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::noname::ili9341_red {

template <int8_t pinLcdCs, int8_t pinTouchCs, int8_t pinLcdDc,
          int8_t pinLcdReset = -1>
class Kmrtm32032Spi : public ComboDevice {
 public:
  Kmrtm32032Spi(Orientation orientation = Orientation(),
                decltype(SPI)& spi = SPI)
      : spi_(spi), display_(spi), touch_() {
    display_.setOrientation(orientation);
  }

  void initTransport() { spi_.begin(); }

  void initTransport(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_.begin(sck, miso, mosi);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  TouchCalibration touch_calibration() override {
    return TouchCalibration(269, 249, 3829, 3684,
                            roo_display::Orientation::LeftDown());
  }

 private:
  decltype(SPI)& spi_;
  roo_display::Ili9341spi<pinLcdCs, pinLcdDc, pinLcdReset> display_;
  roo_display::TouchXpt2046<pinTouchCs> touch_;
};

}  // namespace roo_display::products::noname::ili9341_red

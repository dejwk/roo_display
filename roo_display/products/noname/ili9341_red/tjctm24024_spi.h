#pragma once

// http://www.lcdwiki.com/2.4inch_SPI_Module_ILI9341_SKU:MSP2402
// Maker: ???
// SKU: MSP2402
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
//
// Sample script:
//
// #include <Arduino.h>
//
// #include "roo_display.h"
// #include "roo_display/backlit/esp32_ledc.h"
// #include "roo_display/shape/basic.h"
// #include "roo_display/products/noname/ili9341_red/tjctm24024_spi.h"
//
// using namespace roo_display;
//
// // Override the constants to match your configuration.
// static const int kPinSpiSck = 18;
// static const int kPinSpiMiso = 19;
// static const int kPinSpiMosi = 23;
//
// static const int kPinLcdCs = 5;
// static const int kPinTouchCs = 2;
// static const int kPinLcdDc = 17;
// static const int kPinLcdReset = 27;
//
// static const int kPinLcdBl = 16;
//
// static const int kBlLedcChannel = 1;
//
// products::noname::ili9341_red::Tjctm24024Spi<kPinLcdCs, kPinTouchCs,
// kPinLcdDc,
//                                              kPinLcdReset>
//     display_device;
// Display display(display_device);
//
// LedcBacklit backlit(kPinLcdBl, kBlLedcChannel);
//
// void setup() {
//   display_device.initTransport(kPinSpiSck, kPinSpiMiso, kPinSpiMosi);
//   display.init(color::White);
// }
//
// void loop() {
//   {
//     DrawingContext dc(display);
//     dc.fill(Color(rand(), rand(), rand()));
//   }
//   int16_t x, y;
//   if (display.getTouch(x, y)) {
//     DrawingContext dc(display);
//     dc.draw(Line(0, y, display.width() - 1, y, color::Red));
//     dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
//   }
//   delay(200);
// }

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
class Tjctm24024Spi : public ComboDevice {
 public:
  Tjctm24024Spi(Orientation orientation = Orientation().rotateLeft(),
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
    return TouchCalibration(318, 346, 3824, 3909,
                            roo_display::Orientation::RightUp());
  }

 private:
  decltype(SPI)& spi_;
  roo_display::Ili9341spi<pinLcdCs, pinLcdDc, pinLcdReset> display_;
  roo_display::TouchXpt2046<pinTouchCs> touch_;
};

}  // namespace roo_display::products::noname::ili9341_red

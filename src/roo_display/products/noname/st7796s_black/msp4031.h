#pragma once

// http://www.lcdwiki.com/4.0inch_Capacitive_SPI_Module_ST7796
// Maker: ???
// SKU: MSP4031
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
// #include "roo_display/products/noname/st7796s_black/msp4031.h"
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
// static const int kPinLcdReset = -1;
//
// static const int kPinLcdBl = 16;
//
// static const int kBlLedcChannel = 1;
//
// products::noname::st7796s_black::Msp4031<kPinLcdCs, kPinTouchCs, kPinLcdDc,
//                                              kPinLcdReset>
//     display_device;
// Display display(display_device);
//
// LedcBacklit backlit(kPinLcdBl, kBlLedcChannel);
//
// void setup() {
//   display_device.initTransport(kPinSpiSck, kPinSpiMiso, kPinSpiMosi, kPinSda,
//                                kPinScl);
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
#include "roo_display/driver/st7796s.h"
#include "roo_display/driver/touch_ft6x36.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::noname::st7796s_black {

template <int8_t pinLcdCs, int8_t pinLcdDc, int8_t pinLcdReset = -1>
class Msp4031 : public ComboDevice {
 public:
  Msp4031(Orientation orientation = Orientation().rotateLeft(),
          decltype(SPI) & spi = SPI, decltype(Wire) & wire = Wire)
      : spi_(spi), wire_(wire), display_(spi), touch_(wire) {
    display_.setOrientation(orientation);
  }

  void initTransport() {
    spi_.begin();
    wire_.begin();
  }

  void initTransport(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t sda,
                     uint8_t scl) {
    spi_.begin(sck, miso, mosi);
    wire_.begin(sda, scl);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  TouchCalibration touch_calibration() override {
    return TouchCalibration(0, 0, 319, 479, roo_display::Orientation::RightDown());
  }

 private:
  decltype(SPI) & spi_;
  decltype(Wire) & wire_;
  roo_display::St7796sspi<pinLcdCs, pinLcdDc, pinLcdReset> display_;
  roo_display::TouchFt6x36 touch_;
};

}  // namespace roo_display::products::noname::st7796s_black

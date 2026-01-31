#pragma once

// https://https://www.waveshare.com/0.96inch-lcd-module.htm
// Maker: Waveshare
// SKU: 15868

// Sample script:
//
// #include <Arduino.h>
//
// #include "roo_display.h"
// #include "roo_display/backlit/esp32_ledc.h"
// #include "roo_display/products/waveshare/lcd_module_160x80_096_in.h"
// #include "roo_display/shape/basic.h"
//
// using namespace roo_display;
//
// // Override the constants to match your configuration.
// static const int kPinSpiSck = 18;
// static const int kPinSpiMiso = 19;
// static const int kPinSpiMosi = 23;
//
// static const int kPinLcdCs = 5;
// static const int kPinLcdDc = 2;
// static const int kPinLcdReset = 4;
//
// static const int kPinLcdBl = 14;
//
// static const int kBlLedcChannel = 1;
//
// products::waveshare::LcdModule_160x80<kPinLcdCs, kPinLcdDc, kPinLcdReset>
//     display_device;
// Display display(display_device);
//
// LedcBacklit backlit(kPinLcdBl, kBlLedcChannel);
//
// void setup() {
//   SPI.begin(kPinSpiSck, kPinSpiMiso, kPinSpiMosi);
//   display.init(color::White);
// }
//
// void loop() {
//   {
//     DrawingContext dc(display);
//     dc.fill(Color(rand(), rand(), rand()));
//   }
//   delay(200);
// }

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/st7735.h"
#include "roo_display/hal/spi.h"

namespace roo_display::products::waveshare {

template <int8_t pinCs, int8_t pinDc, int8_t pinReset = -1>
class LcdModule_160x80 : public St7735spi_80x160_inv<pinCs, pinDc, pinReset> {
 public:
  LcdModule_160x80(Orientation orientation = Orientation().rotateLeft(),
                   roo_display::DefaultSpi spi = roo_display::DefaultSpi())
      : St7735spi_80x160_inv<pinCs, pinDc, pinReset>(orientation, spi) {}
};

}  // namespace roo_display::products::waveshare

#pragma once

// https://https://https://www.waveshare.com/1.8inch-lcd-module.htm
// Maker: Waveshare
// SKU: 13892

// Sample script:
//
// #include <Arduino.h>
//
// #include "roo_display.h"
// #include "roo_display/backlit/esp32_ledc.h"
// #include "roo_display/products/waveshare/lcd_module_128x160_18_in.h"
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
// products::waveshare::LcdModule_128x160<kPinLcdCs, kPinLcdDc, kPinLcdReset>
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

#include <SPI.h>

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/st7735.h"

namespace roo_display::products::waveshare {

template <int8_t pinCs, int8_t pinDc, int8_t pinReset = -1>
class LcdModule_128x160 : public St7735spi_128x160<pinCs, pinDc, pinReset> {
 public:
  LcdModule_128x160(Orientation orientation = Orientation(),
                    decltype(SPI)& spi = SPI)
      : St7735spi_128x160<pinCs, pinDc, pinReset>(orientation, spi) {}
};

}  // namespace roo_display::products::waveshare

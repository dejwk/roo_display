#pragma once

// http://www.lcdwiki.com/1.44inch_SPI_Module_ST7735S_SKU:MSP1443
// Maker: ???
// SKU: MSP1443
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
// #include "roo_display/products/noname/st7735_red/tft_module_128x128_144_in.h"
//
// using namespace roo_display;
//
// // Override the constants to match your configuration.
// static const int kPinSpiSck = 18;
// static const int kPinSpiMiso = 19;
// static const int kPinSpiMosi = 23;
//
// static const int kPinLcdCs = 5;
// static const int kPinLcdDc = 17;
// static const int kPinLcdReset = 27;
//
// static const int kPinLcdBl = 16;
//
// static const int kBlLedcChannel = 1;
//
// products::noname::st7735_red::TftModule_128x128_144in<kPinLcdCs, kPinLcdDc,
//                                                       kPinLcdReset>
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

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/st7735.h"

namespace roo_display::products::noname::st7735_red {

template <int8_t pinCs, int8_t pinDc, int8_t pinReset = -1>
class TftModule_128x128_144in
    : public St7735spi_128x128<pinCs, pinDc, pinReset> {
 public:
  TftModule_128x128_144in(Orientation orientation = Orientation().rotateRight(),
                          decltype(SPI)& spi = SPI)
      : St7735spi_128x128<pinCs, pinDc, pinReset>(orientation, spi) {}
};

}  // namespace roo_display::products::noname::st7735_red

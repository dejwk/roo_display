#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/hal/gpio.h"
#include "roo_display/hal/transport.h"

namespace roo_display {
namespace st7789 {

typedef BoundGenericSpi<40000000, MSBFIRST, SPI_MODE3> SpiTransport;

struct Init {
  template <typename Target>
  void init(Target& t, uint8_t xstart, uint8_t xend, uint8_t ystart,
            uint8_t yend, bool inverted) const {
    t.writeCommand(st77xx::SWRESET, {}, 150);
    t.writeCommand(st77xx::SLPOUT, {}, 500);
    t.writeCommand(st77xx::COLMOD, {0x55}, 10);
    t.writeCommand(st77xx::MADCTL, {0x08});
    t.writeCommand(st77xx::CASET, {0x00, xstart, 0x00, xend});
    t.writeCommand(st77xx::RASET, {0x00, ystart, 0x00, yend});
    t.writeCommand(inverted ? st77xx::INVON : st77xx::INVOFF);
    t.writeCommand(st77xx::INVON, {}, 10);
    t.writeCommand(st77xx::NORON, {}, 10);
    t.writeCommand(st77xx::DISPON, {}, 500);
  }
};

}  // namespace st7789

template <int pinCS, int pinDC, int pinRST, int16_t display_width,
          int16_t display_height, int16_t lpad = 0, int16_t tpad = 0,
          int16_t rpad = lpad, int16_t bpad = tpad,
          typename Transport = st7789::SpiTransport,
          typename Gpio = DefaultGpio>
using St7789_Generic = AddrWindowDevice<st77xx::St77xxTarget<
    st7789::Init, pinCS, pinDC, pinRST, display_width, display_height, lpad,
    tpad, rpad, bpad, true, Transport, Gpio>>;

template <int pinCS, int pinDC, int pinRST,
          typename Transport = st7789::SpiTransport,
          typename Gpio = DefaultGpio>
using St7789_240x240 = St7789_Generic<pinCS, pinDC, pinRST, 240, 240, 0, 0, 0,
                                      80, Transport, Gpio>;

}  // namespace roo_display

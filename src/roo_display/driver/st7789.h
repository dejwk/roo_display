#pragma once

#include <cstdint>
#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/transport/spi.h"

namespace roo_display {
namespace st7789 {

typedef SpiSettings<40000000, kSpiMsbFirst, kSpiMode3> DefaultSpiSettings;

struct Init {
  template <typename Target>
  void init(Target& t, uint16_t xstart, uint16_t xend, uint16_t ystart,
            uint16_t yend, bool inverted) const {
    t.writeCommand(st77xx::SWRESET, {}, 150);
    t.writeCommand(st77xx::SLPOUT, {}, 500);
    t.writeCommand(st77xx::COLMOD, {0x55}, 10);
    t.writeCommand(st77xx::MADCTL, {0x08});
    t.writeCommand(st77xx::CASET,
                   {(uint8_t)(xstart >> 8), (uint8_t)(xstart & 0xFF),
                    (uint8_t)(xend >> 8), (uint8_t)(xend & 0xFF)});
    t.writeCommand(st77xx::RASET,
                   {(uint8_t)(ystart >> 8), (uint8_t)(ystart & 0xFF),
                    (uint8_t)(yend >> 8), (uint8_t)(yend & 0xFF)});
    t.writeCommand(inverted ? st77xx::INVON : st77xx::INVOFF);
    t.writeCommand(st77xx::NORON, {}, 10);
    t.writeCommand(st77xx::DISPON, {}, 500);
  }
};

}  // namespace st7789

template <typename Transport, int16_t display_width, int16_t display_height,
          int16_t lpad = 0, int16_t tpad = 0, int16_t rpad = lpad,
          int16_t bpad = tpad>
using St7789_Generic = AddrWindowDevice<
    st77xx::St77xxTarget<Transport, st7789::Init, display_width, display_height,
                         lpad, tpad, rpad, bpad, true>>;

template <int pinCS, int pinDC, int pinRST, int16_t display_width,
          int16_t display_height, int16_t lpad = 0, int16_t tpad = 0,
          int16_t rpad = lpad, int16_t bpad = tpad, typename Spi = DefaultSpi,
          typename SpiSettings = st7789::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7789spi_Generic =
    St7789_Generic<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, Gpio>,
                   display_width, display_height, lpad, tpad, rpad, bpad>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7789::DefaultSpiSettings>
using St7789spi_240x240 =
    St7789spi_Generic<pinCS, pinDC, pinRST, 240, 240, 0, 0, 0, 80, Spi,
                      SpiSettings, DefaultGpio>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7789::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7789spi_240x280 = St7789spi_Generic<pinCS, pinDC, pinRST, 240, 280, 0,
                                            20, 0, 0, Spi, SpiSettings, Gpio>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7789::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7789spi_172x320 =
    St7789spi_Generic<pinCS, pinDC, pinRST, 172, 320, 34, 0, 34, 0, Spi,
                      SpiSettings, DefaultGpio>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7789::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7789spi_135x240 =
    St7789spi_Generic<pinCS, pinDC, pinRST, 135, 240, 0, 40, 53, 0, Spi,
                      SpiSettings, DefaultGpio>;

}  // namespace roo_display

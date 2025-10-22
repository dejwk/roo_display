#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/transport/spi.h"

namespace roo_display {
namespace st7796 {

typedef SpiSettings<40000000, MSBFIRST, SPI_MODE3> DefaultSpiSettings;

struct Init {
  template <typename Target>
  void init(Target& t, uint16_t xstart, uint16_t xend, uint16_t ystart,
            uint16_t yend, bool inverted) const {
    t.writeCommand(st77xx::SWRESET, {}, 150);
    t.writeCommand(st77xx::SLPOUT, {}, 500);
    t.writeCommand(st77xx::COLMOD, {0x55}, 10);
    t.writeCommand(st77xx::MADCTL, {0x00});  // BGR mode.
    t.writeCommand(st77xx::CASET,
                   {(uint8_t)(xstart >> 8), (uint8_t)(xstart & 0xFF),
                    (uint8_t)(xend >> 8), (uint8_t)(xend & 0xFF)});
    t.writeCommand(st77xx::RASET,
                   {(uint8_t)(ystart >> 8), (uint8_t)(ystart & 0xFF),
                    (uint8_t)(yend >> 8), (uint8_t)(yend & 0xFF)});
    t.writeCommand(inverted ? st77xx::INVON : st77xx::INVOFF);
    t.writeCommand(st77xx::NORON, {}, 10);
    t.writeCommand(st77xx::DISPON, {}, 500);

    t.writeCommand(st77xx::CSCON, {0xC3});
    t.writeCommand(st77xx::CSCON, {0x96});

    t.writeCommand(st77xx::DIC, {0x01});  // Dot inversion ON
    t.writeCommand(st77xx::DFC, {0x80, 0x02, 0x3B});

    t.writeCommand(st77xx::DOCA,
                   {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33});

    // VAP(GVDD)=3.85+( vcom+vcom offset), VAN(GVCL)=-3.85+( vcom+vcom offset)
    t.writeCommand(st77xx::PWR2, {0x06});

    // Source driving current level and Gamma driving current level
    t.writeCommand(st77xx::PWR3, {0xA7});

    t.writeCommand(st77xx::VCMPCTL, {0x18}, 120);  // VCOM = 0.9

    // Gamma "+"
    t.writeCommand(st77xx::PGC, {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54,
                                 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B});

    // Gamma "-"
    t.writeCommand(st77xx::NGC, {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43,
                                 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B});

    t.writeCommand(st77xx::CSCON, {0x3C});
    t.writeCommand(st77xx::CSCON, {0x69});
  }
};

}  // namespace st7796

template <typename Transport, int16_t display_width, int16_t display_height,
          int16_t lpad = 0, int16_t tpad = 0, int16_t rpad = lpad,
          int16_t bpad = tpad>
using St7796_Generic = AddrWindowDevice<
    st77xx::St77xxTarget<Transport, st7796::Init, display_width, display_height,
                         lpad, tpad, rpad, bpad, false, true, true>>;

template <int pinCS, int pinDC, int pinRST, int16_t display_width,
          int16_t display_height, int16_t lpad = 0, int16_t tpad = 0,
          int16_t rpad = lpad, int16_t bpad = tpad, typename Spi = DefaultSpi,
          typename SpiSettings = st7796::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7796spi_Generic =
    St7796_Generic<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, Gpio>,
                   display_width, display_height, lpad, tpad, rpad, bpad>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7796::DefaultSpiSettings>
using St7796spi_240x240 =
    St7796spi_Generic<pinCS, pinDC, pinRST, 240, 240, 0, 0, 0, 80, Spi,
                      SpiSettings, DefaultGpio>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7796::DefaultSpiSettings>
using St7796spi_320x480 =
    St7796spi_Generic<pinCS, pinDC, pinRST, 320, 480, 0, 0, 0, 0, Spi,
                      SpiSettings, DefaultGpio>;

}  // namespace roo_display

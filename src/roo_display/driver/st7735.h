#pragma once

#include <Arduino.h>
#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/transport/spi.h"

namespace roo_display {

namespace st7735 {

static const uint32_t kSpiFrequency = 20 * 1000 * 1000;

typedef SpiSettings<kSpiFrequency, kSpiMsbFirst, kSpiMode0> DefaultSpiSettings;

enum Command {
  INVCTR = 0xB4,

  PWCTR1 = 0xC0,
  PWCTR2 = 0xC1,
  PWCTR3 = 0xC2,
  PWCTR4 = 0xC3,
  PWCTR5 = 0xC4,

  VMCTR1 = 0xC5,

  GMCTRP1 = 0xE0,
  GMCTRN1 = 0xE1,
};

struct Init {
  template <typename Target>
  void init(Target& t, int16_t xstart, int16_t xend, int16_t ystart,
            int16_t yend, bool inverted = false) const {
    using namespace ::roo_display::st77xx;
    t.writeCommand(SWRESET, {}, 120);
    t.writeCommand(SLPOUT, {}, 120);
    t.writeCommand(INVCTR, {0x07});
    t.writeCommand(PWCTR1, {0xA2, 0x02, 0x84});
    t.writeCommand(PWCTR2, {0xC5});
    t.writeCommand(PWCTR3, {0x0A, 0x00});
    t.writeCommand(PWCTR4, {0x8A, 0x2A});
    t.writeCommand(PWCTR5, {0x8A, 0xEE});
    t.writeCommand(VMCTR1, {0x0E});
    t.writeCommand(MADCTL, {0xC8});
    t.writeCommand(COLMOD, {0x05});
    // Note: ST7735 has max. 132x162 resolution.
    t.writeCommand(CASET, {0x00, (uint8_t)xstart, 0x00, (uint8_t)xend});
    t.writeCommand(RASET, {0x00, (uint8_t)ystart, 0x00, (uint8_t)yend});
    t.writeCommand(GMCTRP1, {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
                             0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10});
    t.writeCommand(GMCTRN1, {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
                             0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10});
    t.writeCommand(inverted ? INVON : INVOFF);
    t.writeCommand(NORON);
    t.writeCommand(DISPON);
  }
};

}  // namespace st7735

template <typename Transport, int16_t display_width, int16_t display_height,
          int16_t lpad = 0, int16_t tpad = 0, int16_t rpad = lpad,
          int16_t bpad = tpad, bool inverted = false>
using St7735_Generic = AddrWindowDevice<
    st77xx::St77xxTarget<Transport, st7735::Init, display_width, display_height,
                         lpad, tpad, rpad, bpad, inverted>>;

template <int pinCS, int pinDC, int pinRST, int16_t display_width, int16_t display_height,
          int16_t lpad = 0, int16_t tpad = 0, int16_t rpad = lpad,
          int16_t bpad = tpad, bool inverted = false, typename Spi = DefaultSpi,
          typename SpiSettings = st7735::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7735spi_Generic =
    St7735_Generic<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, Gpio>,
                   display_width, display_height, lpad, tpad, rpad, bpad, inverted>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7735::DefaultSpiSettings>
using St7735spi_128x160 =
    St7735spi_Generic<pinCS, pinDC, pinRST, 128, 160, 2, 1, 2, 1, false, Spi, SpiSettings>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7735::DefaultSpiSettings>
using St7735spi_80x160_inv =
    St7735spi_Generic<pinCS, pinDC, pinRST, 80, 160, 26, 1, 26, 1, true, Spi, SpiSettings>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7735::DefaultSpiSettings>
using St7735spi_128x128 =
    St7735spi_Generic<pinCS, pinDC, pinRST, 128, 128, 2, 1, 2, 3, false, Spi, SpiSettings>;

}  // namespace roo_display

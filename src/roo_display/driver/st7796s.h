#pragma once

#include <Arduino.h>
#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/transport/spi.h"

namespace roo_display {

namespace st7796s {

static const uint32_t kSpiFrequency = 80 * 1000 * 1000;

typedef SpiSettings<kSpiFrequency, kSpiMsbFirst, kSpiMode0> DefaultSpiSettings;

enum Command {
  DIC = 0xB4,
  DFC = 0xB6,

  PWCTR1 = 0xC0,
  PWCTR2 = 0xC1,
  PWCTR3 = 0xC2,
  PWCTR4 = 0xC3,
  PWCTR5 = 0xC4,

  VMCTR1 = 0xC5,

  GMCTRP1 = 0xE0,
  GMCTRN1 = 0xE1,

  DOCA = 0xE8,
  CSCON = 0xF0,
};

struct Init {
  template <typename Target>
  void init(Target& t, int16_t xstart, int16_t xend, int16_t ystart,
            int16_t yend, bool inverted = false) const {
    using namespace ::roo_display::st77xx;
    t.writeCommand(SWRESET, {}, 120);
    t.writeCommand(SLPOUT, {}, 120);

    t.writeCommand(CSCON, {0xC3});
    t.writeCommand(CSCON, {0x96});
    t.writeCommand(MADCTL, {0x48});
    t.writeCommand(COLMOD, {0x55});

    t.writeCommand(DIC, {0x01});

    t.writeCommand(DFC, {0x80, 0x02, 0x3B});

    // t.writeCommand(DOCA, {0x40, 0x8A, 0x00, 0x00, 0x25, 0x0A, 0x38, 0x33});
    t.writeCommand(DOCA, {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33});

    t.writeCommand(PWCTR2, {0x06});
    t.writeCommand(PWCTR3, {0xA7});
    t.writeCommand(VMCTR1, {0x18}, 120);

    t.writeCommand(GMCTRP1, {0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54,
                             0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B});
    t.writeCommand(GMCTRN1,
                   {0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43, 0x42, 0x3B,
                    0x16, 0x14, 0x17, 0x1B},
                   120);

    t.writeCommand(CSCON, {0x3C});
    t.writeCommand(CSCON, {0x69});

    t.writeCommand(inverted ? INVON : INVOFF);
    t.writeCommand(NORON);
    t.writeCommand(DISPON);
  }
};

}  // namespace st7796s

template <typename Transport>
using St7796s =
    AddrWindowDevice<st77xx::St77xxTarget<Transport, st7796s::Init, 320, 480, 0,
                                          0, 0, 0, false, true, true>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = st7796s::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using St7796sspi =
    St7796s<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, Gpio>>;

}  // namespace roo_display

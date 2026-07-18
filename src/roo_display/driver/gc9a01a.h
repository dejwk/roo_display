#pragma once

#include <cstdint>

#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/transport/spi.h"

namespace roo_display {

namespace gc9a01a {

static const uint32_t kSpiFrequency = 40 * 1000 * 1000;

typedef SpiSettings<kSpiFrequency, kSpiMsbFirst, kSpiMode0>
    DefaultSpiSettings;

struct Init {
  template <typename Target>
  void init(Target& t, int16_t xstart, int16_t xend, int16_t ystart,
            int16_t yend, bool inverted = true) const {
    using namespace ::roo_display::st77xx;

    t.writeCommand(SWRESET, {}, 120);
    t.writeCommand(SLPOUT, {}, 120);
    t.writeCommand(MADCTL, {BGR});
    t.writeCommand(COLMOD, {0x55});

    // Enable access to the controller's internal registers and apply the
    // vendor-recommended power, gamma, and gate-driver configuration.
    t.writeCommand(0xFE);
    t.writeCommand(0xEF);
    t.writeCommand(0xEB, {0x14});
    t.writeCommand(0x84, {0x60});
    t.writeCommand(0x85, {0xFF});
    t.writeCommand(0x86, {0xFF});
    t.writeCommand(0x87, {0xFF});
    t.writeCommand(0x88, {0x0A});
    t.writeCommand(0x89, {0x23});
    t.writeCommand(0x8A, {0x00});
    t.writeCommand(0x8B, {0x80});
    t.writeCommand(0x8C, {0x01});
    t.writeCommand(0x8D, {0x03});
    t.writeCommand(0x8E, {0xFF});
    t.writeCommand(0x8F, {0xFF});
    t.writeCommand(0x90, {0x08, 0x08, 0x08, 0x08});
    t.writeCommand(0xFF, {0x60, 0x01, 0x04});
    t.writeCommand(0xC3, {0x13});
    t.writeCommand(0xC4, {0x13});
    t.writeCommand(0xC9, {0x30});
    t.writeCommand(0xBE, {0x11});
    t.writeCommand(0xE1, {0x10, 0x0E});
    t.writeCommand(0xDF, {0x21, 0x0C, 0x02});
    t.writeCommand(0xF0, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A});
    t.writeCommand(0xF1, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F});
    t.writeCommand(0xF2, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A});
    t.writeCommand(0xF3, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F});
    t.writeCommand(0xED, {0x1B, 0x0B});
    t.writeCommand(0xAE, {0x77});
    t.writeCommand(0xCD, {0x63});
    t.writeCommand(0x70,
                   {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03});
    t.writeCommand(0xE8, {0x34});
    t.writeCommand(0x60,
                   {0x38, 0x0B, 0x6D, 0x6D, 0x39, 0xF0, 0x6D, 0x6D});
    t.writeCommand(0x61,
                   {0x38, 0xF4, 0x6D, 0x6D, 0x38, 0xF7, 0x6D, 0x6D});
    t.writeCommand(0x62, {0x38, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x38, 0x0F,
                          0x71, 0xEF, 0x70, 0x70});
    t.writeCommand(0x63, {0x38, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x38, 0x13,
                          0x71, 0xF3, 0x70, 0x70});
    t.writeCommand(0x64, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07});
    t.writeCommand(0x66,
                   {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00,
                    0x00});
    t.writeCommand(0x67,
                   {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32,
                    0x98});
    t.writeCommand(0x74, {0x10, 0x45, 0x80, 0x00, 0x00, 0x4E, 0x00});
    t.writeCommand(0x98, {0x3E, 0x07});
    t.writeCommand(0x99, {0x3E, 0x07});

    t.writeCommand(CASET,
                   {static_cast<uint8_t>(xstart >> 8),
                    static_cast<uint8_t>(xstart),
                    static_cast<uint8_t>(xend >> 8),
                    static_cast<uint8_t>(xend)});
    t.writeCommand(RASET,
                   {static_cast<uint8_t>(ystart >> 8),
                    static_cast<uint8_t>(ystart),
                    static_cast<uint8_t>(yend >> 8),
                    static_cast<uint8_t>(yend)});
    t.writeCommand(inverted ? INVON : INVOFF);
    t.writeCommand(NORON, {}, 10);
    t.writeCommand(DISPON, {}, 120);
  }
};

}  // namespace gc9a01a

template <typename Transport>
using Gc9a01a = AddrWindowDevice<st77xx::St77xxTarget<
    Transport, gc9a01a::Init, 240, 240, 0, 0, 0, 0, true, true>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = gc9a01a::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using Gc9a01aspi_240x240 =
    Gc9a01a<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, Gpio>>;

}  // namespace roo_display

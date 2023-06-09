#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/transport/spi.h"

namespace roo_display {

namespace ili9488 {

// Stores R, G, B components in high bits of 3 subsequent bytes. Two low bits of
// each byte are unused.
class Rgb666h {
 public:
  static const int8_t bits_per_pixel = 24;

  inline constexpr Color toArgbColor(uint32_t in) const {
    return Color(((in >> 12) & 0xFC) | (in >> 18),
                 ((in >> 4) & 0xFC) | ((in >> 10) & 0x02),
                 ((in << 0) & 0xFC) | ((in >> 2) & 0x02));
  }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return (truncTo6bit(color.r()) << 18) | (truncTo6bit(color.g()) << 10) |
           (truncTo6bit(color.b()) << 2);
  }

  inline uint16_t rawAlphaBlend(uint32_t bg, Color color) const {
    return fromArgbColor(AlphaBlendOverOpaque(toArgbColor(bg), color));
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

static const int16_t kDefaultWidth = 320;
static const int16_t kDefaultHeight = 480;

static const uint32_t SpiFrequency = 40 * 1000 * 1000;

typedef SpiSettings<SpiFrequency, MSBFIRST, SPI_MODE0> DefaultSpiSettings;

enum Command {
  NOP = 0x00,
  SWRESET = 0x01,

  SLPIN = 0x10,
  SLPOUT = 0x11,

  INVOFF = 0x20,

  DISPOFF = 0x28,
  DISPON = 0x29,
  CASET = 0x2A,
  PASET = 0x2B,
  RAMWR = 0x2C,

  MADCTL = 0x36,
  PIXSET = 0x3A,

  PWCTRL1 = 0xC0,
  PWCTRL2 = 0xC1,
  PWCTRL3 = 0xC2,
  VMCTRL1 = 0xC5,
  VMCTRL2 = 0xC7,

  PGAMCTRL = 0xE0,
  NGAMCTRL = 0xE1,
};

enum MadCtl {
  MY = 0x80,
  MX = 0x40,
  MV = 0x20,
  ML = 0x10,
  BGR = 0x08,
  MH = 0x04
};

template <typename Transport>
class Ili9488Target {
 public:
  typedef Rgb666h ColorMode;
  static constexpr ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN;

  Ili9488Target(uint16_t width = kDefaultWidth,
                uint16_t height = kDefaultHeight)
      : transport_(),
        width_(width),
        height_(height),
        last_x0_(-1),
        last_x1_(-1),
        last_y0_(-1),
        last_y1_(-1) {}

  Ili9488Target(Transport transport, uint16_t width = kDefaultWidth,
                uint16_t height = kDefaultHeight)
      : transport_(std::move(transport)),
        width_(width),
        height_(height),
        last_x0_(-1),
        last_x1_(-1),
        last_y0_(-1),
        last_y1_(-1) {}

  int16_t width() const { return width_; }

  int16_t height() const { return height_; }

  void begin() {
    transport_.beginTransaction();
    transport_.begin();
  }

  void end() {
    transport_.sync();
    transport_.end();
    transport_.endTransaction();
  }

  void init() {
    begin();

    // Set Gamma.
    writeCommand(PGAMCTRL, {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
                            0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F});
    writeCommand(NGAMCTRL, {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
                            0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F});

    writeCommand(PWCTRL1, {0x17, 0x15});
    writeCommand(PWCTRL2, {0x41});

    writeCommand(VMCTRL1, {0x00, 0x12, 0x80});

    // Portrait mode.
    writeCommand(MADCTL, {0x48});

    writeCommand(PIXSET, {0x66});

    // Interface Mode Control.
    writeCommand(0xB0, {0x00});

    // Frame Rate Control.
    writeCommand(0xB1, {0xA0});

    // Display Inversion Control.
    writeCommand(0xB4, {0x02});

    // Display Function Control.
    writeCommand(0xB6, {0x02, 0x02, 0x3B});

    // Entry Mode Set.
    writeCommand(0xB7, {0xC6});

    // Adjust Control 3.
    writeCommand(0xF7, {0xA9, 0x51, 0x2C, 0x82});

    writeCommand(SLPOUT);

    end();

    delay(120);

    begin();
    writeCommand(DISPON);
    end();
  }

  void setOrientation(Orientation orientation) {
    uint8_t d = BGR | (orientation.isXYswapped() ? MV : 0) |
                (orientation.isTopToBottom() ? 0 : MY) |
                (orientation.isRightToLeft() ? 0 : MX);
    writeCommand(MADCTL);
    transport_.write(d);
  }

  void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
      __attribute__((always_inline)) {
    if (last_x0_ != x0 || last_x1_ != x1) {
      transport_.sync();
      writeCommand(CASET);
      transport_.write16x2_async(x0, x1);
      last_x0_ = x0;
      last_x1_ = x1;
    }
    if (last_y0_ != y0 || last_y1_ != y1) {
      transport_.sync();
      writeCommand(PASET);
      transport_.write16x2_async(y0, y1);
      last_y0_ = y0;
      last_y1_ = y1;
    }
    transport_.sync();
    writeCommand(RAMWR);
  }

  void ramWrite(uint32_t* data, size_t count) {
    // Compact the buffer.
    uint8_t* src = (uint8_t*)data;
    uint8_t* dest = (uint8_t*)data;
    size_t byte_count = count * 3;
    while (count-- > 0) {
      ++src;
      *dest++ = *src++;
      *dest++ = *src++;
      *dest++ = *src++;
    }
    // Write the buffer.
    transport_.sync();
    transport_.writeBytes_async((uint8_t*)data, byte_count);
  }

  void ramFill(uint32_t data, size_t count) __attribute__((always_inline)) {
    transport_.fill24be_async(data, count);
  }

 private:
  void writeCommand(uint8_t c) __attribute__((always_inline)) {
    transport_.cmdBegin();
    transport_.write(c);
    transport_.cmdEnd();
  }

  void writeCommand(uint8_t c, const std::initializer_list<uint8_t>& d) {
    writeCommand(c);
    for (uint8_t i : d) transport_.write(i);
  }

  Transport transport_;
  int16_t width_;
  int16_t height_;
  uint16_t last_x0_, last_x1_, last_y0_, last_y1_;
};

}  // namespace ili9488

template <typename Transport>
using Ili9488 = AddrWindowDevice<ili9488::Ili9488Target<Transport>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = ili9488::DefaultSpiSettings>
using Ili9488spi =
    Ili9488<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, DefaultGpio>>;

}  // namespace roo_display

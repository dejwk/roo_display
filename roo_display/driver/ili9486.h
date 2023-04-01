#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/transport/spi.h"

namespace roo_display {

namespace ili9486 {

static const int16_t kDefaultWidth = 320;
static const int16_t kDefaultHeight = 480;

static const uint32_t SpiFrequency = 20 * 1000 * 1000;

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
class Ili9486Target {
 public:
  typedef Rgb565 ColorMode;
  static constexpr ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN;

  Ili9486Target(uint16_t width = kDefaultWidth,
                uint16_t height = kDefaultHeight)
      : transport_(), width_(width), height_(height) {}
  Ili9486Target(Transport transport, uint16_t width = kDefaultWidth,
                uint16_t height = kDefaultHeight)
      : transport_(std::move(transport)),
        width_(width),
        height_(height) {}

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }

  void begin() {
    transport_.beginTransaction();
    transport_.begin();
  }

  void end() {
    transport_.end();
    transport_.endTransaction();
  }

  void setXaddr(uint16_t x0, uint16_t x1) {
    writeCommand(CASET);
    uint8_t xBin[] = {
        0, (uint8_t)(x0 >> 8), 0, (uint8_t)(x0 >> 0),
        0, (uint8_t)(x1 >> 8), 0, (uint8_t)(x1 >> 0),
    };
    transport_.writeBytes(&xBin[0], 8);
  }

  void setYaddr(uint16_t y0, uint16_t y1) {
    writeCommand(PASET);
    uint8_t yBin[] = {
        0, (uint8_t)(y0 >> 8), 0, (uint8_t)(y0 >> 0),
        0, (uint8_t)(y1 >> 8), 0, (uint8_t)(y1 >> 0),
    };
    transport_.writeBytes(&yBin[0], 8);
  }

  void init() {
    // Soft reset.
    begin();
    writeCommand(SWRESET);
    end();
    delay(5);

    begin();
    writeCommand(SLPOUT);  // Sleep out, also SW reset
    delay(120);

    writeCommand(PIXSET, {0x55});  // 16 bits / pixel

    // The first byte affects shadows and the overall brightness; the second -
    // lights.
    writeCommand(PWCTRL1, {0x08, 0x08});

    writeCommand(PWCTRL3, {0x44});  // The highest quality (2H / 8H)
    writeCommand(VMCTRL1, {0x00, 0x00, 0x00, 0x00});  // VCOM Control 1

    writeCommand(PGAMCTRL, {0x0F, 0x1F, 0x1C, 0x0C, 0x0F, 0x08, 0x48, 0x98,
                            0x37, 0x0A, 0x13, 0x04, 0x11, 0x0D, 0x00});
    writeCommand(NGAMCTRL, {0x0F, 0x32, 0x1C, 0x0B, 0x0D, 0x05, 0x50, 0x75,
                            0x37, 0x06, 0x10, 0x03, 0x10, 0x10, 0x00});

    writeCommand(INVOFF);
    writeCommand(MADCTL, {0x48});
    end();
    delay(120);

    begin();
    writeCommand(DISPON);
    end();
    delay(120);
  }

  void setOrientation(Orientation orientation) {
    writeCommand(MADCTL);
    transport_.write16(BGR | (orientation.isXYswapped() ? MV : 0) |
                       (orientation.isTopToBottom() ? 0 : MY) |
                       (orientation.isRightToLeft() ? 0 : MX));
  }

  void beginRamWrite() { writeCommand(RAMWR); }

  void ramWrite(uint16_t* data, size_t count) {
    transport_.writeBytes((uint8_t*)data, count * 2);
  }

  void ramFill(uint16_t data, size_t count) {
    transport_.fill16be(data, count);
  }

 private:
  void writeCommand(uint8_t c) {
    transport_.cmdBegin();
    transport_.write16(c);
    transport_.cmdEnd();
  }

  void writeCommand(uint8_t c, const std::initializer_list<uint8_t>& d) {
    writeCommand(c);
    for (uint8_t i : d) transport_.write16(i);
  }

  Transport transport_;
  int16_t width_;
  int16_t height_;
};

}  // namespace ili9486

template <typename Transport>
using Ili9486 = AddrWindowDevice<ili9486::Ili9486Target<Transport>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = ili9486::DefaultSpiSettings>
using Ili9486spi =
    Ili9486<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, DefaultGpio>>;

}  // namespace roo_display
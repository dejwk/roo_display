#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/hal/gpio.h"
#include "roo_display/hal/transport_bus.h"

namespace roo_display {

namespace ili9341 {

static const int16_t kDefaultWidth = 240;
static const int16_t kDefaultHeight = 320;

static const uint32_t SpiFrequency = 20 * 1000 * 1000;

typedef SpiSettings<SpiFrequency, MSBFIRST, SPI_MODE0> DefaultSpiSettings;

enum Command {
  NOP = 0x00,
  SWRESET = 0x01,

  RDDIDIF = 0x04,
  RDDST = 0x09,
  RDDPM = 0x0A,
  RDDMADCTL = 0x0B,
  RDDCOLMOD = 0x0C,
  RDDIM = 0x0D,
  RDDSM = 0x0E,
  RDDSDR = 0x0F,

  SLPIN = 0x10,
  SLPOUT = 0x11,
  GAMSET = 0x26,
  DISPOFF = 0x28,
  DISPON = 0x29,

  CASET = 0x2A,
  PASET = 0x2B,
  RAMWR = 0x2C,

  MADCTL = 0x36,
  PIXSET = 0x3A,

  FRMCTR1 = 0xB1,
  FRMCTR2 = 0xB2,
  FRMCTR3 = 0xB3,
  INVTR = 0xB4,
  PRCTR = 0xB5,
  DISCTRL = 0xB6,

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

template <int pinCS, int pinDC, int pinRST, typename Transport, typename Gpio>
class Ili9341Target {
 public:
  typedef Rgb565 ColorMode;
  static constexpr ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN;

  Ili9341Target(uint16_t width = ili9341::kDefaultWidth,
                uint16_t height = ili9341::kDefaultHeight)
      : bus_(), transport_(), width_(width), height_(height) {}

  Ili9341Target(Transport transport, uint16_t width = ili9341::kDefaultWidth,
                uint16_t height = ili9341::kDefaultHeight)
      : bus_(),
        transport_(std::move(transport)),
        width_(width),
        height_(height) {}

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }

  void begin() {
    transport_.beginTransaction();
    bus_.begin();
  }

  void end() {
    bus_.end();
    transport_.endTransaction();
  }

  void setXaddr(uint16_t x0, uint16_t x1) {
    writeCommand(CASET);
    transport_.write32(x0 << 16 | x1);
  }

  void setYaddr(uint16_t y0, uint16_t y1) {
    writeCommand(PASET);
    transport_.write32(y0 << 16 | y1);
  }

  void init() {
    begin();

    // bus_.writeCommand(0xEF, {0x03, 0x80, 0x02});
    // bus_.writeCommand(0xCF, {0x00, 0xC1, 0x30});
    // bus_.writeCommand(0xED, {0x64, 0x03, 0x12, 0x81});
    // bus_.writeCommand(0xE8, {0x85, 0x00, 0x78});
    // bus_.writeCommand(0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02});
    // bus_.writeCommand(0xF7, {0x20});
    // bus_.writeCommand(0xEA, {0x00, 0x00});

    writeCommand(PWCTRL1, {0x23});  // VRH[5:0]
    writeCommand(PWCTRL2, {0x10});  // SAP[2:0];BT[3:0]

    writeCommand(VMCTRL1, {0x3E, 0x28});
    writeCommand(VMCTRL2, {0x86});

    writeCommand(MADCTL, {0x48});  // portrait mode

    writeCommand(PIXSET, {0x55});

    // 0x18 79Hz, 0x1B default 70Hz, 0x13 100Hz
    writeCommand(FRMCTR1, {0x00, 0x13});

    writeCommand(DISCTRL, {0x08, 0x82, 0x27});

    // bus_.writeCommand(0xF2, {0x00});  // 3Gamma Function Disable

    // Set Gamma curve.
    writeCommand(GAMSET, {0x01});

    // Set Gamma.
    writeCommand(PGAMCTRL, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                            0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
    writeCommand(NGAMCTRL, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                            0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});

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

  void beginRamWrite() { writeCommand(RAMWR); }

  void ramWrite(uint16_t* data, size_t count) {
    transport_.writeBytes((uint8_t*)data, count * 2);
  }

  void ramFill(uint16_t data, size_t count) {
    transport_.fill16be(data, count);
  }

 private:
  void writeCommand(uint8_t c) {
    bus_.cmdBegin();
    transport_.write(c);
    bus_.cmdEnd();
  }

  void writeCommand(uint8_t c, const std::initializer_list<uint8_t>& d) {
    writeCommand(c);
    for (uint8_t i : d) transport_.write(i);
  }

  TransportBus<pinCS, pinDC, pinRST, Gpio> bus_;
  Transport transport_;
  int16_t width_, height_;
};

}  // namespace ili9341

template <typename Transport, int pinCS, int pinDC, int pinRST,
          typename Gpio = DefaultGpio>
using Ili9341 = AddrWindowDevice<
    ili9341::Ili9341Target<pinCS, pinDC, pinRST, Transport, Gpio>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = ili9341::DefaultSpiSettings,
          typename Gpio = DefaultGpio>
using Ili9341spi =
    Ili9341<BoundSpi<Spi, SpiSettings>, pinCS, pinDC, pinRST, Gpio>;

}  // namespace roo_display

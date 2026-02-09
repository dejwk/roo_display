#pragma once

#include "roo_display/color/color_modes.h"
#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/transport/spi.h"
#include "roo_threads.h"
#include "roo_threads/thread.h"

namespace roo_display {

namespace ili9341 {

static const int16_t kDefaultWidth = 240;
static const int16_t kDefaultHeight = 320;

static const uint32_t kSpiFrequency = 40 * 1000 * 1000;

typedef SpiSettings<kSpiFrequency, kSpiMsbFirst, kSpiMode0> DefaultSpiSettings;

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

template <typename Transport>
class Ili9341Target {
 public:
  typedef Rgb565 ColorMode;
  static constexpr ByteOrder byte_order = roo_io::kBigEndian;

  Ili9341Target(uint16_t width = ili9341::kDefaultWidth,
                uint16_t height = ili9341::kDefaultHeight)
      : transport_(),
        width_(width),
        height_(height),
        last_x0_(-1),
        last_x1_(-1),
        last_y0_(-1),
        last_y1_(-1) {}

  Ili9341Target(Transport transport, uint16_t width = ili9341::kDefaultWidth,
                uint16_t height = ili9341::kDefaultHeight)
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
    transport_.beginWriteOnlyTransaction();
    transport_.begin();
  }

  void end() {
    transport_.sync();
    transport_.end();
    transport_.endTransaction();
  }

  void init() {
    transport_.init();
    begin();

    writeCommand(0xEF, {0x03, 0x80, 0x02});
    writeCommand(0xCF, {0x00, 0xC1, 0x30});
    writeCommand(0xED, {0x64, 0x03, 0x12, 0x81});
    writeCommand(0xE8, {0x85, 0x00, 0x78});
    writeCommand(0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02});
    writeCommand(0xF7, {0x20});
    writeCommand(0xEA, {0x00, 0x00});

    writeCommand(PWCTRL1, {0x23});  // VRH[5:0]
    writeCommand(PWCTRL2, {0x10});  // SAP[2:0];BT[3:0]

    writeCommand(VMCTRL1, {0x3E, 0x28});
    writeCommand(VMCTRL2, {0x86});

    writeCommand(MADCTL, {0x48});  // portrait mode

    writeCommand(PIXSET, {0x55});

    // 0x18 79Hz, 0x1B default 70Hz, 0x13 100Hz
    writeCommand(FRMCTR1, {0x00, 0x13});

    writeCommand(DISCTRL, {0x08, 0x82, 0x27});

    writeCommand(0xF2, {0x00});  // 3Gamma Function Disable

    // Set Gamma curve.
    writeCommand(GAMSET, {0x01});

    // Set Gamma.
    writeCommand(PGAMCTRL, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
                            0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00});
    writeCommand(NGAMCTRL, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
                            0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F});

    writeCommand(SLPOUT);

    end();
    sleep_ms(120);

    begin();
    writeCommand(DISPON);
    end();
  }

  void setOrientation(Orientation orientation) {
    transport_.sync();
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

  void ramWrite(const roo::byte* data, size_t pixel_count)
      __attribute__((always_inline)) {
    transport_.sync();
    transport_.writeBytes_async(data, pixel_count * 2);
  }

  void ramFill(const roo::byte* data, size_t pixel_count)
      __attribute__((always_inline)) {
    // ramFill is called only once per addr window, so we can assume we're
    // synced.
    transport_.fill16_async(data, pixel_count);
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

  void sleep_ms(uint32_t ms) {
    roo::this_thread::sleep_for(roo_time::Millis(ms));
  }

  Transport transport_;
  int16_t width_;
  int16_t height_;
  uint16_t last_x0_, last_x1_, last_y0_, last_y1_;
};

}  // namespace ili9341

template <typename Transport>
using Ili9341 = AddrWindowDevice<ili9341::Ili9341Target<Transport>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = ili9341::DefaultSpiSettings>
using Ili9341spi =
    Ili9341<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, DefaultGpio>>;

}  // namespace roo_display

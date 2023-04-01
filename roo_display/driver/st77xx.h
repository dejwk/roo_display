#pragma once

#include "roo_display/transport/spi.h"

namespace roo_display {
namespace st77xx {

enum Command {
  NOP = 0x00,
  SWRESET = 0x01,
  SLPIN = 0x10,
  SLPOUT = 0x11,
  PTLON = 0x12,
  NORON = 0x13,
  INVOFF = 0x20,
  INVON = 0x21,
  DISPOFF = 0x28,
  DISPON = 0x29,
  CASET = 0x2A,
  RASET = 0x2B,
  RAMWR = 0x2C,

  MADCTL = 0x36,
  COLMOD = 0x3A,
};

enum MadCtl {
  MY = 0x80,
  MX = 0x40,
  MV = 0x20,
  ML = 0x10,
  BGR = 0x08,
  MH = 0x04,
  RGB = 0x00,
};

template <typename Transport, typename Initializer, int16_t display_width,
          int16_t display_height, int16_t lpad = 0, int16_t tpad = 0,
          int16_t rpad = lpad, int16_t bpad = tpad, bool inverted = false>
class St77xxTarget {
 public:
  typedef Rgb565 ColorMode;
  static constexpr ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN;

  St77xxTarget(Transport transport = Transport())
      : transport_(std::move(transport)),
        width_(display_width),
        height_(display_height),
        x_offset_(lpad),
        y_offset_(rpad) {}

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
    transport_.write32((x0 + x_offset_) << 16 | (x1 + x_offset_));
  }

  void setYaddr(uint16_t y0, uint16_t y1) {
    writeCommand(RASET);
    transport_.write32((y0 + y_offset_) << 16 | (y1 + y_offset_));
  }

  void init() {
    Initializer init;
    begin();
    init.init(*this, lpad, display_width + lpad + rpad - 1, tpad,
              display_height + tpad + bpad - 1, inverted);
    end();
  }

  void setOrientation(Orientation orientation) {
    uint8_t d = RGB | (orientation.isXYswapped() ? MV : 0) |
                (orientation.isTopToBottom() ? 0 : MY) |
                (orientation.isLeftToRight() ? 0 : MX);
    writeCommand(MADCTL);
    transport_.write(d);
    int16_t xoffset = orientation.isLeftToRight() ? lpad : rpad;
    int16_t yoffset = orientation.isTopToBottom() ? tpad : bpad;
    x_offset_ = orientation.isXYswapped() ? yoffset : xoffset;
    y_offset_ = orientation.isXYswapped() ? xoffset : yoffset;
  }

  void beginRamWrite() { writeCommand(RAMWR); }

  void ramWrite(uint16_t data) { transport_.write16be(data); }

  void ramWrite(uint16_t* data, size_t count) {
    transport_.writeBytes((uint8_t*)data, count * 2);
  }

  void ramFill(uint16_t data, size_t count) {
    transport_.fill16be(data, count);
  }

  void writeCommand(uint8_t c) {
    transport_.cmdBegin();
    transport_.write(c);
    transport_.cmdEnd();
  }

  void writeCommand(uint8_t c, const std::initializer_list<uint8_t>& d,
                    uint32_t delay_ms = 0) {
    writeCommand(c);
    for (uint8_t i : d) transport_.write(i);
    if (delay_ms > 0) delay(delay_ms);
  }

 private:
  Transport transport_;
  int16_t width_;
  int16_t height_;
  int16_t x_offset_;
  int16_t y_offset_;
};

}  // namespace st77xx
}  // namespace roo_display

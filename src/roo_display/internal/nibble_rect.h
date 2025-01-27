#pragma once

#include <inttypes.h>

#include "roo_display/color/color.h"
#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_io/memory/fill.h"

namespace roo_display {
namespace internal {

// Rectangular nibble (half-byte) area.
class NibbleRect {
 public:
  NibbleRect(roo::byte* buffer, int16_t width_bytes, int16_t height)
      : buffer_(buffer), width_bytes_(width_bytes), height_(height) {}

  roo::byte* buffer() { return buffer_; }
  const roo::byte* buffer() const { return buffer_; }
  int16_t width_bytes() const { return width_bytes_; }
  int16_t width() const { return width_bytes_ * 2; }
  int16_t height() const { return height_; }

  uint8_t get(int16_t x, int16_t y) const {
    return (uint8_t)(buffer_[y * width_bytes_ + x / 2] >> ((1 - (x % 2)) * 4)) &
           0xF;
  }

  void set(int16_t x, int16_t y, uint8_t value) {
    roo::byte& b = buffer_[x / 2 + y * width_bytes()];
    if (x % 2 == 0) {
      b &= roo::byte{0x0F};
      b |= (roo::byte)(value << 4);
    } else {
      b &= roo::byte{0xF0};
      b |= (roo::byte)value;
    }
  }

  // Fills the specified rectangle of the mask with the specified bit value.
  void fillRect(const Box& rect, uint8_t value) {
    if (rect.xMin() == 0 && rect.xMax() == width() - 1) {
      roo_io::NibbleFill(buffer_, rect.yMin() * width(),
                         rect.height() * width(), (roo::byte)value);
    } else {
      int16_t w = rect.width();
      uint32_t offset = rect.xMin() + rect.yMin() * width();
      for (int16_t i = rect.height(); i > 0; --i) {
        roo_io::NibbleFill(buffer_, offset, w, (roo::byte)value);
        offset += width();
      }
    }
  }

 private:
  roo::byte* buffer_;
  int16_t width_bytes_;
  int16_t height_;
};

// Iterates over bits in a specified rectangular sub-window of a nibble rect.
class NibbleRectWindowIterator {
 public:
  NibbleRectWindowIterator(const NibbleRect* rect, int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1)
      : ptr_(rect->buffer() + (y0 * rect->width_bytes() + x0 / 2)),
        nibble_idx_(x0 % 2),
        x_(0),
        width_(x0 == 0 && x1 == rect->width() - 1
                   ? rect->width() * rect->height()
                   : x1 - x0 + 1),
        width_skip_(rect->width() - (x1 - x0 + 1)) {}

  uint8_t next() {
    if (x_ >= width_) {
      ptr_ += (width_skip_ + nibble_idx_) / 2;
      nibble_idx_ = (width_skip_ + nibble_idx_) % 2;
      x_ = 0;
    }
    uint8_t result = (uint8_t)(*ptr_ >> ((1 - nibble_idx_) * 4)) & 0xF;
    ++x_;
    ++nibble_idx_;
    ptr_ += (nibble_idx_ >= 2);
    nibble_idx_ &= 1;
    return result;
  }

 private:
  const roo::byte* ptr_;
  uint8_t nibble_idx_;
  int16_t x_;
  const int16_t width_;
  const int16_t width_skip_;
};

// Adapter to use writer as filler, for a single rectangle.
struct RectFillWriter {
  Color color;
  BufferedRectWriter& writer;
  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) const {
    writer.writeRect(x0, y0, x1, y1, color);
  }
};

}  // namespace internal
}  // namespace roo_display
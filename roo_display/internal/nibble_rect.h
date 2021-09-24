#pragma once

#include <inttypes.h>

#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/color.h"
#include "roo_display/internal/memfill.h"

namespace roo_display {
namespace internal {

// Rectangular nibble (half-byte) area.
class NibbleRect {
 public:
  NibbleRect(uint8_t* buffer, int16_t width_bytes, int16_t height)
      : buffer_(buffer), width_bytes_(width_bytes), height_(height) {}

  uint8_t* buffer() { return buffer_; }
  const uint8_t* buffer() const { return buffer_; }
  int16_t width_bytes() const { return width_bytes_; }
  int16_t width() const { return width_bytes_ * 2; }
  int16_t height() const { return height_; }

  uint8_t get(int16_t x, int16_t y) const {
    return (buffer_[y * width_bytes_ + x / 2] >> ((1 - (x % 2)) * 4)) & 0xF;
  }

  void set(int16_t x, int16_t y, uint8_t value) {
    uint8_t& byte = buffer_[x / 2 + y * width_bytes()];
    if (x % 2 == 0) {
      byte &= 0x0F;
      byte |= (value << 4);
    } else {
      byte &= 0xF0;
      byte |= value;
    }
  }

  // Fills the specified rectangle of the mask with the specified bit value.
  void fillRect(const Box& rect, uint8_t value) {
    if (rect.xMin() == 0 && rect.xMax() == width() - 1) {
      nibble_fill(buffer_, rect.yMin() * width(), rect.height() * width(),
                  value);
    } else {
      int16_t w = rect.width();
      uint32_t offset = rect.xMin() + rect.yMin() * width();
      for (int16_t i = rect.height(); i > 0; --i) {
        nibble_fill(buffer_, offset, w, value);
        offset += width();
      }
    }
  }

 private:
  uint8_t* buffer_;
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
    uint8_t result = (*ptr_ >> ((1 - nibble_idx_) * 4)) & 0xF;
    ++x_;
    ++nibble_idx_;
    ptr_ += (nibble_idx_ >= 2);
    nibble_idx_ &= 1;
    return result;
  }

 private:
  const uint8_t* ptr_;
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
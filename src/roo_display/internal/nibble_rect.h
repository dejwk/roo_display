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

// Scans a row of nibbles starting at (data, phase) for the first nibble that
// differs from 'value'. Returns the offset (in nibbles from start) of the first
// mismatch, or 'count' if all nibbles match.
//
// 'data' points to the byte containing the first nibble.
// 'phase' is 0 if starting at the high nibble, 1 if starting at the low nibble.
// 'count' is the number of nibbles to scan.
// 'value' is the 4-bit nibble value to compare against (0..15).
inline int16_t NibbleFindFirstMismatch(const roo::byte* data, int phase,
                                       int16_t count, uint8_t value) {
  if (count <= 0) return 0;
  int16_t pos = 0;

  // Phase 1: handle leading nibble to align to byte boundary.
  if (phase == 1 && count > 0) {
    uint8_t nibble = (uint8_t)(*data) & 0x0F;
    if (nibble != value) return 0;
    ++data;
    ++pos;
    --count;
  }

  // Phase 2: handle leading bytes to align to uint32_t boundary.
  const uint8_t byte_pattern = (value << 4) | value;
  while (count >= 2 && (reinterpret_cast<uintptr_t>(data) & 3) != 0) {
    if ((uint8_t)(*data) != byte_pattern) {
      // Mismatch in high or low nibble.
      uint8_t hi = (uint8_t)(*data) >> 4;
      if (hi != value) return pos;
      return pos + 1;
    }
    ++data;
    pos += 2;
    count -= 2;
  }

  // Phase 3: bulk u32 comparison (8 nibbles per word).
  const uint32_t word_pattern = byte_pattern * 0x01010101u;
  while (count >= 8) {
    uint32_t word;
    __builtin_memcpy(&word, data, 4);
    if (word != word_pattern) {
      // Mismatch somewhere in this word; find exact nibble.
      for (int i = 0; i < 4; ++i) {
        if ((uint8_t)(data[i]) != byte_pattern) {
          uint8_t hi = (uint8_t)(data[i]) >> 4;
          if (hi != value) return pos + i * 2;
          return pos + i * 2 + 1;
        }
      }
    }
    data += 4;
    pos += 8;
    count -= 8;
  }

  // Phase 4: trailing bytes.
  while (count >= 2) {
    if ((uint8_t)(*data) != byte_pattern) {
      uint8_t hi = (uint8_t)(*data) >> 4;
      if (hi != value) return pos;
      return pos + 1;
    }
    ++data;
    pos += 2;
    count -= 2;
  }

  // Phase 5: trailing nibble.
  if (count > 0) {
    uint8_t hi = (uint8_t)(*data) >> 4;
    if (hi != value) return pos;
    ++pos;
  }

  return pos;
}

// Returns the nibble at the given nibble_index from the start of 'data'.
// nibble_index 0 is the high nibble of data[0], 1 is the low nibble of
// data[0], 2 is the high nibble of data[1], etc.
__attribute__((always_inline)) inline uint8_t NibbleAt(const roo::byte* data,
                                                       int16_t nibble_index) {
  return (uint8_t)(data[nibble_index / 2] >> ((1 - (nibble_index & 1)) * 4)) &
         0xF;
}

// Scans a row of nibbles starting at (data, phase) for the first nibble that
// equals 'value'. Returns the offset (in nibbles from start) of the first
// match, or 'count' if no nibble matches.
//
// Simple per-nibble scan — mismatch streaks are typically short, so the
// overhead of a bulk scan would outweigh its benefit here.
__attribute__((always_inline)) inline int16_t NibbleFindFirstMatch(
    const roo::byte* data, int phase, int16_t count, uint8_t value) {
  for (int16_t i = 0; i < count; ++i) {
    if (NibbleAt(data, phase + i) == value) return i;
  }
  return count;
}

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
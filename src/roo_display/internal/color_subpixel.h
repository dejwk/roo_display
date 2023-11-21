#pragma once

#include <inttypes.h>

#include "roo_display/color/color.h"
#include "roo_display/color/traits.h"

namespace roo_display {

// In case of color modes that store multiple pixels in a single byte,
// specifies whether the leftmost pixel is mapped to the high or low order
// bits.
enum ColorPixelOrder {
  COLOR_PIXEL_ORDER_MSB_FIRST,
  COLOR_PIXEL_ORDER_LSB_FIRST
};

// Utility class for color modes whose pixels_per_byte > 1. Allows callers to
// read and write raw colors at various pixel indexes. This functionality is
// here, rather than directly in the ColorMode classes, because it is
// repetitive: identical for any color mode with the same pixels_per_byte.
//
// The template contract (implemented by specializations and partial
// specializations, below) is the following:
//
// struct SubPixelColorHelper {
//   // Writes the specified raw_color (which is expected to be stored
//   // in the low bits) to the specified target byte, at the specified
//   // index location (from left-to-right, counting from zero). The
//   // index is in the range [0, pixels_per_byte - 1].
//   void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index);
//
//   // Reads a raw color from the specified byte, at the specified index
//   // (from left-to-right, counting from zero).
//   uint8_t ReadSubPixelColor(const uint8_t source, int index);
//
//   // Returns a byte resulting from filling pixels_per_byte pixels
//   // to the same specified raw color.
//   uint8_t RawToFullByte(uint8_t raw_color);
//
//   // Takes a byte specifying a sequence of raw colors, and converts
//   // them all to ARGB8, storing the resulting pixels_per_byte colors
//   // in the specified result array.
//   inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
//                                     Color *result);
// };

template <typename ColorMode, ColorPixelOrder pixel_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte>
struct SubPixelColorHelper;

// Specialization that works for for Monochrome LSB-first.
template <typename ColorMode>
struct SubPixelColorHelper<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST, 8> {
  void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index) {
    if (raw_color) {
      *target |= (0x01 << index);
    } else {
      *target &= ~(0x01 << index);
    }
  }
  uint8_t ReadSubPixelColor(const uint8_t source, int index) {
    return (source & (0x01 << index)) != 0;
  }
  uint8_t RawToFullByte(uint8_t raw_color) { return raw_color ? 0xFF : 0x00; }
  inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
                                    Color *result) const {
    result[0] = mode.toArgbColor((in & 0x01) != 0);
    result[1] = mode.toArgbColor((in & 0x02) != 0);
    result[2] = mode.toArgbColor((in & 0x04) != 0);
    result[3] = mode.toArgbColor((in & 0x08) != 0);
    result[4] = mode.toArgbColor((in & 0x10) != 0);
    result[5] = mode.toArgbColor((in & 0x20) != 0);
    result[6] = mode.toArgbColor((in & 0x40) != 0);
    result[7] = mode.toArgbColor((in & 0x80) != 0);
  }
};

// Specialization that works for for Monochrome MSB-first.
template <typename ColorMode>
struct SubPixelColorHelper<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, 8> {
  void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index) {
    if (raw_color) {
      *target |= (0x80 >> index);
    } else {
      *target &= ~(0x80 >> index);
    }
  }
  uint8_t ReadSubPixelColor(const uint8_t source, int index) {
    return (source & (0x80 >> index)) != 0;
  }
  uint8_t RawToFullByte(uint8_t raw_color) { return raw_color ? 0xFF : 0x00; }
  inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
                                    Color *result) const {
    result[0] = mode.toArgbColor((in & 0x80) != 0);
    result[1] = mode.toArgbColor((in & 0x40) != 0);
    result[2] = mode.toArgbColor((in & 0x20) != 0);
    result[3] = mode.toArgbColor((in & 0x10) != 0);
    result[4] = mode.toArgbColor((in & 0x08) != 0);
    result[5] = mode.toArgbColor((in & 0x04) != 0);
    result[6] = mode.toArgbColor((in & 0x02) != 0);
    result[7] = mode.toArgbColor((in & 0x01) != 0);
  }
};

// Specialization that works for for 4bpp color modes.
template <typename ColorMode>
struct SubPixelColorHelper<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST, 2> {
  void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index) {
    uint8_t mask = 0xF0 >> (index << 2);
    *target &= mask;
    *target |= (raw_color << (index << 2));
  }
  uint8_t ReadSubPixelColor(const uint8_t source, int index) {
    return (source >> (index << 2)) & 0x0F;
  }
  uint8_t RawToFullByte(uint8_t raw_color) { return raw_color * 0x11; }
  inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
                                    Color *result) const {
    result[0] = mode.toArgbColor(in & 0x0F);
    result[1] = mode.toArgbColor(in >> 4);
  }
};

template <typename ColorMode>
struct SubPixelColorHelper<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, 2> {
  void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index) {
    uint8_t mask = (0x0F << (index << 2));
    *target &= mask;
    *target |= (raw_color << ((1 - index) << 2));
  }
  uint8_t ReadSubPixelColor(const uint8_t source, int index) {
    return (source >> ((1 - index) << 2)) & 0x0F;
  }
  uint8_t RawToFullByte(uint8_t raw_color) { return raw_color * 0x11; }
  inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
                                    Color *result) const {
    result[0] = mode.toArgbColor(in >> 4);
    result[1] = mode.toArgbColor(in & 0x0F);
  }
};

// Specialization that works for for 2bpp color modes.
template <typename ColorMode>
struct SubPixelColorHelper<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST, 4> {
  void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index) {
    uint8_t mask = 0x03 << (index << 1);
    *target &= ~mask;
    *target |= (raw_color << (index << 1));
  }
  uint8_t ReadSubPixelColor(const uint8_t source, int index) {
    return (source >> (index << 1)) & 0x03;
  }
  uint8_t RawToFullByte(uint8_t raw_color) { return raw_color * 0x55; }
  inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
                                    Color *result) const {
    result[0] = mode.toArgbColor((in >> 0) & 0x03);
    result[1] = mode.toArgbColor((in >> 2) & 0x03);
    result[2] = mode.toArgbColor((in >> 4) & 0x03);
    result[3] = mode.toArgbColor(in >> 6);
  }
};

template <typename ColorMode>
struct SubPixelColorHelper<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, 4> {
  void applySubPixelColor(uint8_t raw_color, uint8_t *target, int index) {
    uint8_t mask = (0x03 << ((3 - index) << 1));
    *target &= ~mask;
    *target |= (raw_color << ((3 - index) << 1));
  }
  uint8_t ReadSubPixelColor(const uint8_t source, int index) {
    return (source >> ((3 - index) << 1)) & 0x03;
  }
  uint8_t RawToFullByte(uint8_t raw_color) { return raw_color * 0x55; }
  inline void ReadSubPixelColorBulk(const ColorMode &mode, uint8_t in,
                                    Color *result) const {
    result[0] = mode.toArgbColor(in >> 6);
    result[1] = mode.toArgbColor((in >> 4) & 0x03);
    result[2] = mode.toArgbColor((in >> 2) & 0x03);
    result[3] = mode.toArgbColor((in >> 0) & 0x03);
  }
};

}  // namespace roo_display

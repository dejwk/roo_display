#pragma once

#include <inttypes.h>
#include <stddef.h>

#include "roo_backport/byte.h"
#include "roo_display/color/color.h"
#include "roo_display/color/traits.h"
#include "roo_io/data/byte_order.h"

namespace roo_display {

template <typename ColorMode, roo_io::ByteOrder byte_order,
          typename Enable = void>
struct ColorIo {
  void store(Color src, roo::byte *dest,
             const ColorMode &mode = ColorMode()) const;
  Color load(const roo::byte *src, const ColorMode &mode = ColorMode()) const;
};

// 1-byte pixels.
template <typename ColorMode, roo_io::ByteOrder byte_order>
struct ColorIo<ColorMode, byte_order,
               std::enable_if_t<ColorTraits<ColorMode>::bytes_per_pixel == 1 &&
                                ColorTraits<ColorMode>::pixels_per_byte == 1>> {
  void store(Color src, roo::byte *dest,
             const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    dest[0] = static_cast<roo::byte>(mode.fromArgbColor(src));
  }
  Color load(const roo::byte *src, const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    return mode.toArgbColor(static_cast<uint8_t>(src[0]));
  }
};

// 2-byte pixels.
template <typename ColorMode, roo_io::ByteOrder byte_order>
struct ColorIo<ColorMode, byte_order,
               std::enable_if_t<ColorTraits<ColorMode>::bytes_per_pixel == 2>> {
  void store(Color src, roo::byte *dest,
             const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    *(uint16_t *)dest =
        roo_io::hto<uint16_t, byte_order>(mode.fromArgbColor(src));
  }
  Color load(const roo::byte *src, const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    return mode.toArgbColor(
        roo_io::toh<uint16_t, byte_order>(*(const uint16_t *)src));
  }
};

// 3-byte pixels.
template <typename ColorMode, roo_io::ByteOrder byte_order>
struct ColorIo<ColorMode, byte_order,
               std::enable_if_t<ColorTraits<ColorMode>::bytes_per_pixel == 3 &&
                                ColorTraits<ColorMode>::pixels_per_byte == 1>> {
  void store(Color src, roo::byte *dest,
             const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    uint32_t raw = mode.fromArgbColor(src);
    if constexpr (byte_order == roo_io::kBigEndian) {
      dest[0] = static_cast<roo::byte>(raw >> 16);
      dest[1] = static_cast<roo::byte>(raw >> 8);
      dest[2] = static_cast<roo::byte>(raw >> 0);
    } else {
      dest[0] = static_cast<roo::byte>(raw >> 0);
      dest[1] = static_cast<roo::byte>(raw >> 8);
      dest[2] = static_cast<roo::byte>(raw >> 16);
    }
  }
  Color load(const roo::byte *src, const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    if constexpr (byte_order == roo_io::kBigEndian) {
      return mode.toArgbColor((static_cast<uint32_t>(src[0]) << 16) |
                              (static_cast<uint32_t>(src[1]) << 8) |
                              (static_cast<uint32_t>(src[2]) << 0));
    } else {
      return mode.toArgbColor((static_cast<uint32_t>(src[0]) << 0) |
                              (static_cast<uint32_t>(src[1]) << 8) |
                              (static_cast<uint32_t>(src[2]) << 16));
    }
  }
};

// 4-byte pixels.
template <typename ColorMode, roo_io::ByteOrder byte_order>
struct ColorIo<ColorMode, byte_order,
               std::enable_if_t<ColorTraits<ColorMode>::bytes_per_pixel == 4>> {
  void store(Color src, roo::byte *dest,
             const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    *(uint32_t *)dest =
        roo_io::hto<uint32_t, byte_order>(mode.fromArgbColor(src));
  }
  Color load(const roo::byte *src, const ColorMode &mode = ColorMode()) const
      __attribute__((always_inline)) {
    return mode.toArgbColor(
        roo_io::toh<uint32_t, byte_order>(*(const uint32_t *)src));
  }
};

// In case of color modes that store multiple pixels in a single byte,
// specifies whether the leftmost pixel is mapped to the high or low order
// bits.
enum ColorPixelOrder {
  COLOR_PIXEL_ORDER_MSB_FIRST,
  COLOR_PIXEL_ORDER_LSB_FIRST
};

// Utility class for color modes whose pixels_per_byte > 1. Allows callers to
// load and store raw colors at various pixel indexes. This functionality is
// here, rather than directly in the ColorMode classes, because it is
// repetitive: identical for any color mode with the same pixels_per_byte.
//
// The template contract (implemented by specializations and partial
// specializations, below) is the following:
//
// struct SubByteColorIo {
//   // Stores the specified raw_color (which is expected to be stored
//   // in the low bits) to the specified target byte, at the specified
//   // index location (from left-to-right, counting from zero). The
//   // index is in the range [0, pixels_per_byte - 1].
//   void storeRaw(uint8_t raw_color, roo::byte *target, int index);
//
//   // Loads a raw color from the specified byte, at the specified index
//   // (from left-to-right, counting from zero).
//   uint8_t loadRaw(const roo::byte source, int index);
//
//   // Returns a byte resulting from filling pixels_per_byte pixels
//   // to the same specified raw color.
//   roo::byte expandRaw(uint8_t raw_color);
//
//   // Takes a byte specifying a sequence of raw colors, and converts
//   // them all to ARGB8, storing the resulting pixels_per_byte colors
//   // in the specified result array.
//   inline void loadBulk(const ColorMode &mode, roo::byte in,
//                           Color *result);
// };

template <typename ColorMode, ColorPixelOrder pixel_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte>
struct SubByteColorIo;

// Specialization that works for for Monochrome LSB-first.
template <typename ColorMode>
struct SubByteColorIo<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST, 8> {
  void storeRaw(uint8_t raw_color, roo::byte *target, int index) const {
    if (raw_color) {
      *target |= (roo::byte{0x01} << index);
    } else {
      *target &= ~(roo::byte{0x01} << index);
    }
  }
  uint8_t loadRaw(const roo::byte source, int index) {
    return (source & (roo::byte{0x01} << index)) != roo::byte{0};
  }
  roo::byte expandRaw(uint8_t raw_color) {
    return raw_color ? roo::byte{0xFF} : roo::byte{0x00};
  }
  inline void loadBulk(const ColorMode &mode, roo::byte in,
                       Color *result) const {
    result[0] = mode.toArgbColor((in & roo::byte{0x01}) != roo::byte{0});
    result[1] = mode.toArgbColor((in & roo::byte{0x02}) != roo::byte{0});
    result[2] = mode.toArgbColor((in & roo::byte{0x04}) != roo::byte{0});
    result[3] = mode.toArgbColor((in & roo::byte{0x08}) != roo::byte{0});
    result[4] = mode.toArgbColor((in & roo::byte{0x10}) != roo::byte{0});
    result[5] = mode.toArgbColor((in & roo::byte{0x20}) != roo::byte{0});
    result[6] = mode.toArgbColor((in & roo::byte{0x40}) != roo::byte{0});
    result[7] = mode.toArgbColor((in & roo::byte{0x80}) != roo::byte{0});
  }
};

// Specialization that works for for Monochrome MSB-first.
template <typename ColorMode>
struct SubByteColorIo<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, 8> {
  void storeRaw(uint8_t raw_color, roo::byte *target, int index) {
    if (raw_color) {
      *target |= (roo::byte{0x80} >> index);
    } else {
      *target &= ~(roo::byte{0x80} >> index);
    }
  }
  uint8_t loadRaw(const roo::byte source, int index) {
    return (source & (roo::byte{0x80} >> index)) != roo::byte{0};
  }
  roo::byte expandRaw(uint8_t raw_color) {
    return raw_color ? roo::byte{0xFF} : roo::byte{0x00};
  }
  inline void loadBulk(const ColorMode &mode, roo::byte in,
                       Color *result) const {
    result[0] = mode.toArgbColor((in & roo::byte{0x80}) != roo::byte{0});
    result[1] = mode.toArgbColor((in & roo::byte{0x40}) != roo::byte{0});
    result[2] = mode.toArgbColor((in & roo::byte{0x20}) != roo::byte{0});
    result[3] = mode.toArgbColor((in & roo::byte{0x10}) != roo::byte{0});
    result[4] = mode.toArgbColor((in & roo::byte{0x08}) != roo::byte{0});
    result[5] = mode.toArgbColor((in & roo::byte{0x04}) != roo::byte{0});
    result[6] = mode.toArgbColor((in & roo::byte{0x02}) != roo::byte{0});
    result[7] = mode.toArgbColor((in & roo::byte{0x01}) != roo::byte{0});
  }
};

// Specialization that works for for 4bpp color modes.
template <typename ColorMode>
struct SubByteColorIo<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST, 2> {
  void storeRaw(uint8_t raw_color, roo::byte *target, int index) {
    roo::byte mask = roo::byte{0xF0} >> (index << 2);
    *target &= mask;
    *target |= (roo::byte)(raw_color << (index << 2));
  }
  uint8_t loadRaw(const roo::byte source, int index) {
    return (uint8_t)((source >> (index << 2)) & roo::byte{0x0F});
  }
  roo::byte expandRaw(uint8_t raw_color) {
    return (roo::byte)(raw_color * 0x11);
  }
  inline void loadBulk(const ColorMode &mode, roo::byte in,
                       Color *result) const {
    result[0] = mode.toArgbColor((uint8_t)(in & roo::byte{0x0F}));
    result[1] = mode.toArgbColor((uint8_t)(in >> 4));
  }
};

template <typename ColorMode>
struct SubByteColorIo<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, 2> {
  void storeRaw(uint8_t raw_color, roo::byte *target, int index) {
    roo::byte mask = (roo::byte{0x0F} << (index << 2));
    *target &= mask;
    *target |= (roo::byte)(raw_color << ((1 - index) << 2));
  }
  uint8_t loadRaw(const roo::byte source, int index) {
    return (uint8_t)((source >> ((1 - index) << 2)) & roo::byte{0x0F});
  }
  roo::byte expandRaw(uint8_t raw_color) {
    return (roo::byte)(raw_color * 0x11);
  }
  inline void loadBulk(const ColorMode &mode, roo::byte in,
                       Color *result) const {
    result[0] = mode.toArgbColor((uint8_t)(in >> 4));
    result[1] = mode.toArgbColor((uint8_t)(in & roo::byte{0x0F}));
  }
};

// Specialization that works for for 2bpp color modes.
template <typename ColorMode>
struct SubByteColorIo<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST, 4> {
  void storeRaw(uint8_t raw_color, roo::byte *target, int index) {
    roo::byte mask = roo::byte{0x03} << (index << 1);
    *target &= ~mask;
    *target |= (roo::byte)(raw_color << (index << 1));
  }
  uint8_t loadRaw(const roo::byte source, int index) {
    return (uint8_t)((source >> (index << 1)) & roo::byte{0x03});
  }
  roo::byte expandRaw(uint8_t raw_color) {
    return (roo::byte)(raw_color * 0x55);
  }
  inline void loadBulk(const ColorMode &mode, roo::byte in,
                       Color *result) const {
    result[0] = mode.toArgbColor((uint8_t)((in >> 0) & roo::byte{0x03}));
    result[1] = mode.toArgbColor((uint8_t)((in >> 2) & roo::byte{0x03}));
    result[2] = mode.toArgbColor((uint8_t)((in >> 4) & roo::byte{0x03}));
    result[3] = mode.toArgbColor((uint8_t)(in >> 6));
  }
};

template <typename ColorMode, roo_io::ByteOrder byte_order,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          typename Enable = void>
struct ColorRectIo;

// Default implementation for sub-byte modes.
template <typename ColorMode, roo_io::ByteOrder byte_order,
          ColorPixelOrder pixel_order>
struct ColorRectIo<
    ColorMode, byte_order, pixel_order,
    std::enable_if_t<ColorTraits<ColorMode>::pixels_per_byte != 1>> {
  void interpret(const roo::byte *data, size_t row_width_bytes, int16_t x0,
                 int16_t y0, int16_t x1, int16_t y1, Color *output,
                 const ColorMode &mode = ColorMode()) const {
    constexpr int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte;
    const uint16_t width = x1 - x0 + 1;
    const uint16_t height = y1 - y0 + 1;
    SubByteColorIo<ColorMode, pixel_order> io;
    if (x0 == 0 && width * ColorMode::bits_per_pixel == row_width_bytes * 8) {
      const roo::byte *ptr = data + y0 * row_width_bytes;
      const uint32_t total_pixels = width * height;
      const uint32_t total_bytes = total_pixels / pixels_per_byte;
      for (uint32_t i = 0; i < total_bytes; ++i) {
        io.loadBulk(mode, *ptr++, output);
        output += pixels_per_byte;
      }
      return;
    }
    const int16_t byte_index = x0 / pixels_per_byte;
    const int16_t init_pixel_index = x0 % pixels_per_byte;
    const int16_t full_limit = ((x1 + 1) / pixels_per_byte) * pixels_per_byte;
    const int16_t trailing_count = (x1 + 1) % pixels_per_byte;

    const roo::byte *row = data + y0 * row_width_bytes;
    for (int16_t y = y0; y <= y1; ++y) {
      const roo::byte *pos = row + byte_index;
      row += row_width_bytes;  // For next row.
      int16_t x = x0;
      for (int16_t pidx = init_pixel_index; pidx < pixels_per_byte && x <= x1;
           ++pidx, ++x) {
        uint8_t raw = io.loadRaw(*pos, pidx);
        *output++ = mode.toArgbColor(raw);
      }
      if (x > x1) continue;
      ++pos;
      for (; x < full_limit; x += pixels_per_byte) {
        io.loadBulk(mode, *pos++, output);
        output += pixels_per_byte;
      }
      if (x > x1) continue;
      for (int16_t pidx = 0; pidx < trailing_count; ++pidx) {
        uint8_t raw = io.loadRaw(*pos, pidx);
        *output++ = mode.toArgbColor(raw);
      }
    }
  }
};

// Specialization for full-byte modes.
template <typename ColorMode, roo_io::ByteOrder byte_order,
          ColorPixelOrder pixel_order>
struct ColorRectIo<
    ColorMode, byte_order, pixel_order,
    std::enable_if_t<ColorTraits<ColorMode>::pixels_per_byte == 1>> {
  void interpret(const roo::byte *data, size_t row_width_bytes, int16_t x0,
                 int16_t y0, int16_t x1, int16_t y1, Color *output,
                 const ColorMode &mode = ColorMode()) const {
    constexpr size_t bytes_per_pixel = ColorTraits<ColorMode>::bytes_per_pixel;
    const int16_t width = x1 - x0 + 1;
    const int16_t height = y1 - y0 + 1;
    ColorIo<ColorMode, byte_order> io;
    if (x0 == 0 && width * bytes_per_pixel == row_width_bytes) {
      const roo::byte *ptr = data + y0 * row_width_bytes;
      uint32_t count = static_cast<uint32_t>(width) * height;
      while (count-- > 0) {
        *output++ = io.load(ptr, mode);
        ptr += bytes_per_pixel;
      }
      return;
    }
    const roo::byte *row = data + y0 * row_width_bytes + x0 * bytes_per_pixel;
    for (int16_t y = y0; y <= y1; ++y) {
      const roo::byte *pixel = row;
      for (int16_t x = x0; x <= x1; ++x) {
        *output++ = io.load(pixel, mode);
        pixel += bytes_per_pixel;
      }
      row += row_width_bytes;
    }
  }
};

template <typename ColorMode>
struct SubByteColorIo<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, 4> {
  void storeRaw(uint8_t raw_color, roo::byte *target, int index) {
    roo::byte mask = (roo::byte{0x03} << ((3 - index) << 1));
    *target &= ~mask;
    *target |= (roo::byte)(raw_color << ((3 - index) << 1));
  }
  uint8_t loadRaw(const roo::byte source, int index) {
    return (uint8_t)((source >> ((3 - index) << 1)) & roo::byte{0x03});
  }
  roo::byte expandRaw(uint8_t raw_color) {
    return (roo::byte)(raw_color * 0x55);
  }
  inline void loadBulk(const ColorMode &mode, roo::byte in,
                       Color *result) const {
    result[0] = mode.toArgbColor((uint8_t)(in >> 6));
    result[1] = mode.toArgbColor((uint8_t)((in >> 4) & roo::byte{0x03}));
    result[2] = mode.toArgbColor((uint8_t)((in >> 2) & roo::byte{0x03}));
    result[3] = mode.toArgbColor((uint8_t)((in >> 0) & roo::byte{0x03}));
  }
};

}  // namespace roo_display

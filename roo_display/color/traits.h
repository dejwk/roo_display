#pragma once

#include <inttypes.h>

namespace roo_display {

template <typename ColorMode, int8_t bits_per_pixel = ColorMode::bits_per_pixel>
struct ColorTraits;

template <typename ColorMode>
struct ColorTraits<ColorMode, 1> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 8;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 2> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 4;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 4> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 2;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 8> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 16> {
  typedef uint16_t storage_type;
  static const int8_t bytes_per_pixel = 2;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 24> {
  typedef uint32_t storage_type;
  static const int8_t bytes_per_pixel = 3;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 32> {
  typedef uint32_t storage_type;
  static const int8_t bytes_per_pixel = 4;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
using ColorStorageType = typename ColorTraits<ColorMode>::storage_type;

}  // namespace roo_display

#pragma once

#include <inttypes.h>

namespace roo_display {

/// Traits for a color mode's storage characteristics.
template <typename ColorMode, int8_t bits_per_pixel = ColorMode::bits_per_pixel>
struct ColorTraits;

/// Storage traits for 1-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 1> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 8;
};

/// Storage traits for 2-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 2> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 4;
};

/// Storage traits for 4-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 4> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 2;
};

/// Storage traits for 8-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 8> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 1;
};

/// Storage traits for 16-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 16> {
  typedef uint16_t storage_type;
  static const int8_t bytes_per_pixel = 2;
  static const int8_t pixels_per_byte = 1;
};

/// Storage traits for 24-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 24> {
  typedef uint32_t storage_type;
  static const int8_t bytes_per_pixel = 3;
  static const int8_t pixels_per_byte = 1;
};

/// Storage traits for 32-bit modes.
template <typename ColorMode>
struct ColorTraits<ColorMode, 32> {
  typedef uint32_t storage_type;
  static const int8_t bytes_per_pixel = 4;
  static const int8_t pixels_per_byte = 1;
};

/// Convenience alias for the raw storage type of a color mode.
template <typename ColorMode>
using ColorStorageType = typename ColorTraits<ColorMode>::storage_type;

}  // namespace roo_display

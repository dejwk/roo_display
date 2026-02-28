#pragma once

#include "roo_logging.h"

namespace roo_display {

// In case of color modes that store multiple pixels in a single byte,
// specifies whether the leftmost pixel is mapped to the high or low order
// bits.
enum class ColorPixelOrder {
  kMsbFirst,
  kLsbFirst
};

roo_logging::Stream& operator<<(roo_logging::Stream& os, ColorPixelOrder order);

[[deprecated("Use ColorPixelOrder::kMsbFirst instead")]]
constexpr auto COLOR_PIXEL_ORDER_MSB_FIRST = ColorPixelOrder::kMsbFirst;
[[deprecated("Use ColorPixelOrder::kLsbFirst instead")]]
constexpr auto COLOR_PIXEL_ORDER_LSB_FIRST = ColorPixelOrder::kLsbFirst;

}  // namespace roo_display

#pragma once

namespace roo_display {

// In case of color modes that store multiple pixels in a single byte,
// specifies whether the leftmost pixel is mapped to the high or low order
// bits.
enum ColorPixelOrder {
  COLOR_PIXEL_ORDER_MSB_FIRST,
  COLOR_PIXEL_ORDER_LSB_FIRST
};

}  // namespace roo_display

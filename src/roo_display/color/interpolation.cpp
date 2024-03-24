#include "roo_display/color/interpolation.h"

namespace roo_display {

Color InterpolateColors(Color c1, Color c2, int16_t fraction) {
  int16_t f2 = fraction;
  int16_t f1 = 256 - fraction;
  uint16_t c1_a = c1.a();
  uint16_t c2_a = c2.a();
  uint32_t a_mult = (c1_a * f1 + c2_a * f2);
  uint32_t a = a_mult / 256;
  if (c1_a == c2_a) {
    // Common case, e.g. both colors opaque. Leave fractions as-is.
  } else if (c1_a == 0) {
    f1 = 0;
    f2 = 256;
  } else if (c2_a == 0) {
    f1 = 256;
    f2 = 0;
  } else {
    f2 = (uint32_t)256 * f2 * c2_a / a_mult;
    f1 = 256 - f2;
  }

  uint32_t mask1 = 0x00FF00FF;
  uint32_t mask2 = 0x0000FF00;
  uint32_t rgb =
      (((((c1.asArgb() & mask1) * f1) + ((c2.asArgb() & mask1) * f2)) / 256) &
       mask1) |
      (((((c1.asArgb() & mask2) * f1) + ((c2.asArgb() & mask2) * f2)) / 256) &
       mask2);
  return Color(a << 24 | rgb);
}

Color InterpolateOpaqueColors(Color c1, Color c2, int16_t fraction) {
  int16_t f2 = fraction;
  int16_t f1 = 256 - fraction;
  uint32_t mask1 = 0x00FF00FF;
  uint32_t mask2 = 0x0000FF00;
  uint32_t rgb =
      (((((c1.asArgb() & mask1) * f1) + ((c2.asArgb() & mask1) * f2)) / 256) &
       mask1) |
      (((((c1.asArgb() & mask2) * f1) + ((c2.asArgb() & mask2) * f2)) / 256) &
       mask2);
  return Color(0xFF000000 | rgb);
}

}  // namespace roo_display

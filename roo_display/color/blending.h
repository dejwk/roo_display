#pragma once

#include <inttypes.h>

#include "roo_display/color/color.h"
#include "roo_display/color/paint_mode.h"

namespace roo_display {

namespace internal {

// In practice, seems to be measurably faster than actually dividing by 255
// and counting on the compiler to optimize.
inline constexpr uint8_t __div_255(uint32_t arg) {
  return ((arg + 1) * 257) >> 16;
}

}  // namespace internal

// Calculates alpha-blending of the foreground color (fgc) over the background
// color (bgc), ignoring background color's alpha, as if it is fully opaque.
inline Color AlphaBlendOverOpaque(Color bgc, Color fgc) {
  uint16_t alpha = fgc.a();
  uint16_t inv_alpha = alpha ^ 0xFF;
  uint8_t r =
      (uint8_t)(internal::__div_255(alpha * fgc.r() + inv_alpha * bgc.r()));
  uint8_t g =
      (uint8_t)(internal::__div_255(alpha * fgc.g() + inv_alpha * bgc.g()));
  uint8_t b =
      (uint8_t)(internal::__div_255(alpha * fgc.b() + inv_alpha * bgc.b()));

  // https://stackoverflow.com/questions/12011081
  // The implementation commented out below is slightly faster (14ms vs 16ms
  // in a microbenchmark) than the implementation above, but it is one-off
  // for some important cases, like fg = 0x00000000 (transparency), and
  // bg = 0xFFFFFFFF (white), which should produce white but produces
  // 0xFFFEFEFE.
  //
  // uint16_t alpha = fgc.a() + 1;
  // uint16_t inv_alpha = 256 - alpha;
  // uint8_t r = (uint8_t)((alpha * fgc.r() + inv_alpha * bgc.r()) >> 8);
  // uint8_t g = (uint8_t)((alpha * fgc.g() + inv_alpha * bgc.g()) >> 8);
  // uint8_t b = (uint8_t)((alpha * fgc.b() + inv_alpha * bgc.b()) >> 8);
  //
  return Color(0xFF000000 | r << 16 | g << 8 | b);
}

// Calculates alpha-blending of the foreground color (fgc) over the background
// color (bgc), in a general case, where bgc might be semi-transparent.
// If the background is (or should be treated as) opaque, use
// AlphaBlendOverOpaque() instead.
inline Color AlphaBlend(Color bgc, Color fgc) {
  uint16_t front_alpha = fgc.a();
  if (front_alpha == 0xFF) {
    return fgc;
  }
  uint16_t back_alpha = bgc.a();
  if (back_alpha == 0xFF) {
    return AlphaBlendOverOpaque(bgc, fgc);
  }
  if (front_alpha == 0) {
    return bgc;
  }
  if (back_alpha == 0) {
    return fgc;
  }
  // Blends a+b so that, when later applied over c, the result is the same as if
  // they were applied in succession; e.g. c+(a+b) == (c+a)+b.
  uint16_t tmp = back_alpha * front_alpha;
  uint16_t alpha = back_alpha + front_alpha - ((tmp + (tmp >> 8)) >> 8);
  uint16_t front_multi = ((front_alpha + 1) << 8) / (alpha + 1);
  uint16_t back_multi = 256 - front_multi;

  uint8_t r = (uint8_t)((front_multi * fgc.r() + back_multi * bgc.r()) >> 8);
  uint8_t g = (uint8_t)((front_multi * fgc.g() + back_multi * bgc.g()) >> 8);
  uint8_t b = (uint8_t)((front_multi * fgc.b() + back_multi * bgc.b()) >> 8);
  return Color(alpha << 24 | r << 16 | g << 8 | b);
}

inline Color CombineColors(Color bgc, Color fgc, PaintMode paint_mode) {
  switch (paint_mode) {
    case PAINT_MODE_REPLACE: {
      return fgc;
    }
    case PAINT_MODE_BLEND: {
      return AlphaBlend(bgc, fgc);
    }
    default: {
      return Color(0);
    }
  }
}

// Ratio is expected in the range of [0, 128], inclusively.
// The resulting color = c1 * (ratio/128) + c2 * (1 - ratio/128).
// Use ratio = 64 for the arithmetic average.
inline constexpr Color AverageColors(Color c1, Color c2, uint8_t ratio) {
  return Color(
      (((uint8_t)c1.a() * ratio + (uint8_t)c2.a() * (128 - ratio)) >> 7) << 24 |
      (((uint8_t)c1.r() * ratio + (uint8_t)c2.r() * (128 - ratio)) >> 7) << 16 |
      (((uint8_t)c1.g() * ratio + (uint8_t)c2.g() * (128 - ratio)) >> 7) << 8 |
      (((uint8_t)c1.b() * ratio + (uint8_t)c2.b() * (128 - ratio)) >> 7) << 0);
}

}  // namespace roo_display

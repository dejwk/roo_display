#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/traits.h"

namespace roo_display {

// Interpolates between the two colors using the specified `fraction`, in the
// 0-256 range. 0 means result = c1; 256 means result = c2; 128 means 50/50.
Color InterpolateColors(Color c1, Color c2, int16_t fraction);

// Interpolates between the two colors, presumed opaque, using the specified
// `fraction`, in the 0-256 range. 0 means result = c1; 256 means result = c2;
// 128 means 50/50.
Color InterpolateOpaqueColors(Color c1, Color c2, int16_t fraction);

// Default implementation, specialized in color_modes.h.
// Given that color_mode.transparency() is usually a compile-time constant, this
// implementation should perform reasonably well with all RGB, ARGB, and RGBA
// modes.
template <typename ColorMode>
struct RawColorInterpolator {
  ColorStorageType<ColorMode> operator()(ColorStorageType<ColorMode> c1,
                                         ColorStorageType<ColorMode> c2,
                                         uint16_t fraction,
                                         const ColorMode& color_mode) const {
    Color c1_argb = color_mode.toArgbColor(c1);
    Color c2_argb = color_mode.toArgbColor(c2);
    return color_mode.fromArgbColor(
        color_mode.transparency() == TRANSPARENCY_NONE
            ? InterpolateOpaqueColors(c1_argb, c2_argb, fraction)
            : InterpolateColors(c1_argb, c2_argb, fraction));
  }
};

}  // namespace roo_display

#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/traits.h"

namespace roo_display {

/// Interpolate between two colors with `fraction` in [0, 256].
///
/// 0 means result = c1; 256 means result = c2; 128 means 50/50.
Color InterpolateColors(Color c1, Color c2, int16_t fraction);

/// Interpolate between two opaque colors with `fraction` in [0, 256].
Color InterpolateOpaqueColors(Color c1, Color c2, int16_t fraction);

/// Equivalent to InterpolateColors(Transparent, c, fraction_color).
Color InterpolateColorWithTransparency(Color c, int16_t fraction_color);

/// Equivalent to InterpolateColors(Transparent, c, fraction_color) for opaque c.
inline Color InterpolateOpaqueColorWithTransparency(Color c,
                                                    int16_t fraction_color) {
  return c.withA(fraction_color);
}

/// Default raw color interpolator (specialized in color_modes.h).
///
/// Uses the color mode transparency to choose an appropriate interpolation.
template <typename ColorMode>
struct RawColorInterpolator {
  Color operator()(ColorStorageType<ColorMode> c1,
                   ColorStorageType<ColorMode> c2, uint16_t fraction,
                   const ColorMode& color_mode) const {
    Color c1_argb = color_mode.toArgbColor(c1);
    Color c2_argb = color_mode.toArgbColor(c2);
    return color_mode.transparency() == TRANSPARENCY_NONE
               ? InterpolateOpaqueColors(c1_argb, c2_argb, fraction)
               : InterpolateColors(c1_argb, c2_argb, fraction);
  }
};

}  // namespace roo_display

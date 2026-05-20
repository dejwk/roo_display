#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/traits.h"

namespace roo_display {

/// @brief Interpolates between two colors using alpha-aware weighting.
/// @param c1 Color returned when `fraction` is 0.
/// @param c2 Color returned when `fraction` is 256.
/// @param fraction Interpolation factor in the range [0, 256].
/// @return The interpolated color, including the blended alpha channel.
Color InterpolateColors(Color c1, Color c2, int16_t fraction);

/// @brief Interpolates between two fully opaque colors.
/// @param c1 Color returned when `fraction` is 0.
/// @param c2 Color returned when `fraction` is 256.
/// @param fraction Interpolation factor in the range [0, 256].
/// @return The opaque interpolation of `c1` and `c2`.
Color InterpolateOpaqueColors(Color c1, Color c2, int16_t fraction);

/// @brief Scales a color's alpha channel toward transparency.
/// @param c Source color.
/// @param fraction_color Opacity factor in the range [0, 256].
/// @return The equivalent of interpolating from transparent to `c`.
Color InterpolateColorWithTransparency(Color c, int16_t fraction_color);

/// @brief Assigns an alpha byte to a color that is already known to be opaque.
/// @param c Source color.
/// @param fraction_color Alpha value to assign, typically in the range
///     [0, 255].
/// @return `c` with its alpha channel replaced by `fraction_color`.
inline Color InterpolateOpaqueColorWithTransparency(Color c,
                                                    int16_t fraction_color) {
  return c.withA(fraction_color);
}

/// @brief Mixes two colors using explicit weights and alpha-aware blending.
/// @param c1 First source color.
/// @param c1_fraction Weight applied to `c1` in the range [0, 256].
/// @param c2 Second source color.
/// @param c2_fraction Weight applied to `c2` in the range [0, 256].
/// @return The weighted mix of `c1` and `c2`, or transparent if the weighted
///     alpha rounds down to zero.
Color MixColors(Color c1, uint16_t c1_fraction, Color c2, uint16_t c2_fraction);

/// @brief Default raw color interpolator.
///
/// Converts raw color values to ARGB8888 and chooses an interpolation strategy
/// based on the color mode's transparency support.
///
/// @tparam ColorMode Raw color mode to interpolate.
template <typename ColorMode>
struct RawColorInterpolator {
  /// @brief Interpolates two raw color values.
  /// @param c1 Raw color returned when `fraction` is 0.
  /// @param c2 Raw color returned when `fraction` is 256.
  /// @param fraction Interpolation factor in the range [0, 256].
  /// @param color_mode Color mode used for conversion and transparency rules.
  /// @return The interpolated ARGB color.
  Color operator()(ColorStorageType<ColorMode> c1,
                   ColorStorageType<ColorMode> c2, uint16_t fraction,
                   const ColorMode& color_mode) const {
    Color c1_argb = color_mode.toArgbColor(c1);
    Color c2_argb = color_mode.toArgbColor(c2);
    return color_mode.transparency() == TransparencyMode::kNone
               ? InterpolateOpaqueColors(c1_argb, c2_argb, fraction)
               : InterpolateColors(c1_argb, c2_argb, fraction);
  }
};

}  // namespace roo_display

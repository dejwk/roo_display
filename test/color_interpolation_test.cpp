#include <ostream>

#include "gtest/gtest.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/interpolation.h"

namespace roo_display {

std::ostream& operator<<(std::ostream& os, const Color& color) {
  return os << "argb:0x" << std::hex << color.asArgb() << std::dec;
}

TEST(ColorInterpolation, InterpolateColorsReturnsSameColorWhenInputsMatch) {
  Color color(0x80402010);

  EXPECT_EQ(color, InterpolateColors(color, color, 73));
}

TEST(ColorInterpolation, InterpolateColorsInterpolatesWhenAlphaMatches) {
  Color c1(128, 64, 96, 32);
  Color c2(128, 192, 32, 96);

  EXPECT_EQ(Color(128, 96, 80, 48), InterpolateColors(c1, c2, 64));
}

TEST(ColorInterpolation, InterpolateColorsUsesSecondRgbWhenFirstIsTransparent) {
  Color c1(0, 1, 2, 3);
  Color c2(192, 100, 150, 200);

  EXPECT_EQ(Color(96, 100, 150, 200), InterpolateColors(c1, c2, 128));
}

TEST(ColorInterpolation, InterpolateColorsUsesFirstRgbWhenSecondIsTransparent) {
  Color c1(128, 64, 96, 32);
  Color c2(0, 1, 2, 3);

  EXPECT_EQ(Color(96, 64, 96, 32), InterpolateColors(c1, c2, 64));
}

TEST(ColorInterpolation, InterpolateColorsReweightsRgbForDifferentAlpha) {
  Color c1(64, 10, 20, 30);
  Color c2(192, 90, 100, 110);

  EXPECT_EQ(Color(128, 70, 80, 90), InterpolateColors(c1, c2, 128));
}

TEST(ColorInterpolation,
     InterpolateOpaqueColorsReturnsSameColorWhenInputsMatch) {
  Color color(10, 20, 30);

  EXPECT_EQ(color, InterpolateOpaqueColors(color, color, 200));
}

TEST(ColorInterpolation, InterpolateOpaqueColorsInterpolatesOpaqueRgb) {
  Color c1(20, 40, 60);
  Color c2(100, 120, 140);

  EXPECT_EQ(Color(60, 80, 100), InterpolateOpaqueColors(c1, c2, 128));
}

TEST(ColorInterpolation, InterpolateColorWithTransparencyScalesAlpha) {
  Color color(200, 10, 20, 30);

  EXPECT_EQ(Color(50, 10, 20, 30),
            InterpolateColorWithTransparency(color, 64));
}

TEST(ColorInterpolation,
     InterpolateOpaqueColorWithTransparencyAssignsAlphaByte) {
  Color color(10, 20, 30);

  EXPECT_EQ(Color(77, 10, 20, 30),
            InterpolateOpaqueColorWithTransparency(color, 77));
}

TEST(ColorInterpolation, MixColorsReturnsTransparentWhenWeightedAlphaIsZero) {
  Color c1(1, 20, 40, 60);
  Color c2(0, 100, 120, 140);

  EXPECT_EQ(color::Transparent, MixColors(c1, 1, c2, 0));
}

TEST(ColorInterpolation, MixColorsInterpolatesWeightedColors) {
  Color c1(128, 20, 40, 60);
  Color c2(128, 100, 120, 140);

  EXPECT_EQ(Color(128, 60, 80, 100), MixColors(c1, 128, c2, 128));
}

TEST(ColorInterpolation,
     RawColorInterpolatorUsesOpaqueInterpolationForOpaqueModes) {
  Rgb888 color_mode;
  RawColorInterpolator<Rgb888> interpolator;
  Color c1(20, 40, 60);
  Color c2(100, 120, 140);

  EXPECT_EQ(InterpolateOpaqueColors(c1, c2, 128),
            interpolator(color_mode.fromArgbColor(c1),
                         color_mode.fromArgbColor(c2), 128, color_mode));
}

TEST(ColorInterpolation,
     RawColorInterpolatorUsesAlphaAwareInterpolationForTransparentModes) {
  Argb8888 color_mode;
  RawColorInterpolator<Argb8888> interpolator;
  Color c1(64, 10, 20, 30);
  Color c2(192, 90, 100, 110);

  EXPECT_EQ(InterpolateColors(c1, c2, 128),
            interpolator(color_mode.fromArgbColor(c1),
                         color_mode.fromArgbColor(c2), 128, color_mode));
}

}  // namespace roo_display
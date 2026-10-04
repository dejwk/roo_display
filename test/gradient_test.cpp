#include "roo_display/color/gradient.h"

#include <vector>

#include "testing.h"

namespace roo_display {
namespace {

// Compares rectangle/uniform hints to independently sampled pixel colors.
void CheckGradientRect(const LinearGradient& gradient, const Box& box) {
  std::vector<Color> pixels(box.area(), color::Green);
  bool uniform = gradient.readColorRect(box.xMin(), box.yMin(), box.xMax(),
                                        box.yMax(), pixels.data());
  Color hint = color::Green;
  bool hinted = gradient.readUniformColorRect(box.xMin(), box.yMin(),
                                              box.xMax(), box.yMax(), &hint);
  size_t offset = 0;
  for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
    for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
      Color expected;
      gradient.readColors(&x, &y, 1, &expected);
      EXPECT_EQ(pixels[uniform ? 0 : offset], expected);
      if (hinted) {
        EXPECT_EQ(hint, expected);
      }
      ++offset;
    }
  }
}

}  // namespace

// Verifies opacity hints account for interpolation and fading boundaries and
// reach every gradient drawable, including opaque periodic gradients.
TEST(Gradient, TransparencyIncludesInterpolatedAlphaAndBoundary) {
  const Box box(0, 0, 8, 8);
  for (ColorGradient::Boundary boundary :
       {ColorGradient::Boundary::kExtended, ColorGradient::Boundary::kPeriodic,
        ColorGradient::Boundary::kTruncated}) {
    for (uint8_t a : {0, 128, 255}) {
      for (uint8_t b : {0, 128, 255}) {
        ColorGradient colors(
            {{0, Color(a, 200, 0, 0)}, {4, Color(b, 0, 100, 255)}}, boundary);
        TransparencyMode expected = TransparencyMode::kFull;
        if (boundary != ColorGradient::Boundary::kTruncated && a == b) {
          if (a == 255) expected = TransparencyMode::kNone;
          if (a == 0) expected = TransparencyMode::kCrude;
        }
        EXPECT_EQ(colors.getTransparencyMode(), expected);
        LinearGradient linear({0, 0}, 0, 1, colors, box);
        RadialGradient radial({0, 0}, colors, box);
        RadialGradientSq squared({0, 0}, colors, box);
        AngularGradient angular({0, 0}, colors, box);
        for (const Rasterizable* gradient : std::vector<const Rasterizable*>{
                 &linear, &radial, &squared, &angular}) {
          EXPECT_EQ(gradient->getTransparencyMode(), expected);
        }
      }
    }
  }
}

// Verifies constant rows, columns, and points report uniform output without
// filling caller storage, while skewed rectangles retain every pixel.
TEST(Gradient, UniformRowsColumnsAndGeneralRectangles) {
  for (ColorGradient::Boundary boundary :
       {ColorGradient::Boundary::kExtended, ColorGradient::Boundary::kPeriodic,
        ColorGradient::Boundary::kTruncated}) {
    ColorGradient colors(
        {{0, color::Red}, {2, Color(0x80203040)}, {5, color::Blue}}, boundary);
    for (float dx : {-1.0f, 0.0f, 0.75f}) {
      for (float dy : {-0.5f, 0.0f, 1.0f}) {
        LinearGradient gradient({2, -1}, dx, dy, colors, Box(-8, -8, 8, 8));
        for (Box box : {Box(-3, -2, 5, 4), Box(-3, 2, 5, 2), Box(2, -3, 2, 5),
                        Box(2, 2, 2, 2)}) {
          CheckGradientRect(gradient, box);
          Color result[81];
          std::fill_n(result, 81, color::Green);
          bool uniform = gradient.readColorRect(box.xMin(), box.yMin(),
                                                box.xMax(), box.yMax(), result);
          bool constant = (dx == 0.0f || box.width() == 1) &&
                          (dy == 0.0f || box.height() == 1);
          EXPECT_EQ(uniform, constant);
          if (uniform) {
            EXPECT_EQ(result[1], color::Green);
          }
        }
      }
    }
  }
}

}  // namespace roo_display


#include <math.h>

#include <array>
#include <fstream>
#include <string>

#include "roo_display/color/color.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/shape/smooth_internal.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

namespace {

constexpr int kThinOutlineReproScale = 32;
constexpr int kThinOutlineReproPixelSize = 32;
constexpr int kThinOutlineReproGapPixels = 1;
constexpr int kThinOutlineReproWidth = 6;
constexpr int kThinOutlineReproHeight = 6;
constexpr Color kThinOutlineReproInterior(0x95E07A5F);
constexpr float kAlphaContrastReproThickness = 0.5f;
constexpr Color kAlphaContrastOutlineOpaque(0xFFFF3B30);
constexpr Color kAlphaContrastInteriorOpaque(0xFF00C7D9);

struct ThinOutlineReproCase {
  float thickness;
  Color outline;
  const char* basename;
};

struct ThinOutlineAlphaContrastReproCase {
  Color outline;
  Color interior;
  const char* basename;
};

const std::array<ThinOutlineReproCase, 4> kThinOutlineReproCases = {{
    {0.25f, color::Black, "smooth_roundrect_thin_outline_025_opaque_repro"},
    {0.5f, color::Black, "smooth_roundrect_thin_outline_050_opaque_repro"},
    {0.25f, color::Black.withA(0x95),
     "smooth_roundrect_thin_outline_025_translucent_repro"},
    {0.5f, color::Black.withA(0x95),
     "smooth_roundrect_thin_outline_050_translucent_repro"},
}};

const std::array<ThinOutlineAlphaContrastReproCase, 4>
    kThinOutlineAlphaContrastReproCases = {{
        {kAlphaContrastOutlineOpaque, kAlphaContrastInteriorOpaque,
         "smooth_roundrect_thin_outline_050_outline100_interior100_repro"},
        {kAlphaContrastOutlineOpaque, kAlphaContrastInteriorOpaque.withA(0x40),
         "smooth_roundrect_thin_outline_050_outline100_interior025_repro"},
        {kAlphaContrastOutlineOpaque.withA(0x40), kAlphaContrastInteriorOpaque,
         "smooth_roundrect_thin_outline_050_outline025_interior100_repro"},
        {kAlphaContrastOutlineOpaque.withA(0x40),
         kAlphaContrastInteriorOpaque.withA(0x40),
         "smooth_roundrect_thin_outline_050_outline025_interior025_repro"},
    }};

Color PixelAt(const FakeOffscreen<Argb8888>& offscreen, int16_t x, int16_t y) {
  return offscreen.buffer()[y * offscreen.raw_width() + x];
}

FakeOffscreen<Argb8888> RenderThinOutlineRepro(float thickness, Color outline,
                                               Color interior, int scale,
                                               Color background = color::White) {
  const float coordinate_offset = 0.5f * (scale - 1);
  const SmoothShape shape = SmoothThickRoundRect(
      0.5f * scale + coordinate_offset, 0.5f * scale + coordinate_offset,
      4.5f * scale + coordinate_offset, 4.5f * scale + coordinate_offset,
      1.0f * scale, thickness * scale, outline, interior);
  FakeOffscreen<Argb8888> result(Box(0, 0, kThinOutlineReproWidth * scale - 1,
                                     kThinOutlineReproHeight * scale - 1),
                                 background);
  result.begin();
  Surface surface(
      result, 0, 0,
      Box(0, 0, result.extents().width() - 1, result.extents().height() - 1),
      false, background, FillMode::kVisible, BlendingMode::kSourceOver);
  surface.drawObject(shape);
  result.end();
  result.resetPixelDrawCount();
  return result;
}

FakeOffscreen<Argb8888> RenderThinOutlineRepro(float thickness, Color outline,
                                               int scale,
                                               Color background = color::White) {
  return RenderThinOutlineRepro(thickness, outline, kThinOutlineReproInterior,
                                scale, background);
}

Color AveragedPixel(const FakeOffscreen<Argb8888>& offscreen, int x0, int y0,
                    int factor) {
  uint32_t a = 0;
  uint32_t r = 0;
  uint32_t g = 0;
  uint32_t b = 0;
  for (int y = 0; y < factor; ++y) {
    for (int x = 0; x < factor; ++x) {
      Color pixel = PixelAt(offscreen, x0 + x, y0 + y);
      a += pixel.a();
      r += pixel.r();
      g += pixel.g();
      b += pixel.b();
    }
  }
  const uint32_t count = factor * factor;
  return Color((((a + count / 2) / count) << 24) |
               (((r + count / 2) / count) << 16) |
               (((g + count / 2) / count) << 8) | ((b + count / 2) / count));
}

Color PremultipliedAveragedPixel(const FakeOffscreen<Argb8888>& offscreen,
                                 int x0, int y0, int factor) {
  uint64_t alpha_sum = 0;
  uint64_t red_alpha_sum = 0;
  uint64_t green_alpha_sum = 0;
  uint64_t blue_alpha_sum = 0;
  for (int y = 0; y < factor; ++y) {
    for (int x = 0; x < factor; ++x) {
      const Color pixel = PixelAt(offscreen, x0 + x, y0 + y);
      const uint32_t alpha = pixel.a();
      alpha_sum += alpha;
      red_alpha_sum += pixel.r() * alpha;
      green_alpha_sum += pixel.g() * alpha;
      blue_alpha_sum += pixel.b() * alpha;
    }
  }
  const uint32_t count = factor * factor;
  const uint8_t alpha = static_cast<uint8_t>((alpha_sum + count / 2) / count);
  if (alpha == 0) return color::Transparent;

  const uint8_t red =
      static_cast<uint8_t>((red_alpha_sum + alpha_sum / 2) / alpha_sum);
  const uint8_t green =
      static_cast<uint8_t>((green_alpha_sum + alpha_sum / 2) / alpha_sum);
  const uint8_t blue =
      static_cast<uint8_t>((blue_alpha_sum + alpha_sum / 2) / alpha_sum);
  return Color((static_cast<uint32_t>(alpha) << 24) |
               (static_cast<uint32_t>(red) << 16) |
               (static_cast<uint32_t>(green) << 8) |
               static_cast<uint32_t>(blue));
}

FakeOffscreen<Argb8888> DownsampleByAveraging(
    const FakeOffscreen<Argb8888>& offscreen, int factor) {
  const int16_t width = offscreen.raw_width() / factor;
  const int16_t height = offscreen.raw_height() / factor;
  FakeOffscreen<Argb8888> result(Box(0, 0, width - 1, height - 1),
                                 color::White);
  for (int16_t y = 0; y < height; ++y) {
    for (int16_t x = 0; x < width; ++x) {
      result.writePixel(
          BlendingMode::kSource, x, y,
          AveragedPixel(offscreen, x * factor, y * factor, factor));
    }
  }
  result.resetPixelDrawCount();
  return result;
}

FakeOffscreen<Argb8888> DownsampleByPremultipliedAveraging(
    const FakeOffscreen<Argb8888>& offscreen, int factor) {
  const int16_t width = offscreen.raw_width() / factor;
  const int16_t height = offscreen.raw_height() / factor;
  FakeOffscreen<Argb8888> result(Box(0, 0, width - 1, height - 1),
                                 color::Transparent);
  for (int16_t y = 0; y < height; ++y) {
    for (int16_t x = 0; x < width; ++x) {
      result.writePixel(
          BlendingMode::kSource, x, y,
          PremultipliedAveragedPixel(offscreen, x * factor, y * factor,
                                     factor));
    }
  }
  result.resetPixelDrawCount();
  return result;
}

float NormalizedRmse(const FakeOffscreen<Argb8888>& actual,
                     const FakeOffscreen<Argb8888>& expected) {
  EXPECT_EQ(actual.raw_width(), expected.raw_width());
  EXPECT_EQ(actual.raw_height(), expected.raw_height());
  uint64_t sum_sq = 0;
  const int pixel_count = actual.raw_width() * actual.raw_height();
  for (int y = 0; y < actual.raw_height(); ++y) {
    for (int x = 0; x < actual.raw_width(); ++x) {
      const Color actual_color = PixelAt(actual, x, y);
      const Color expected_color = PixelAt(expected, x, y);
      const int da = actual_color.a() - expected_color.a();
      const int dr = actual_color.r() - expected_color.r();
      const int dg = actual_color.g() - expected_color.g();
      const int db = actual_color.b() - expected_color.b();
      sum_sq += da * da + dr * dr + dg * dg + db * db;
    }
  }
  return sqrt(static_cast<double>(sum_sq) /
              (pixel_count * 4.0 * 255.0 * 255.0));
}

void WriteRgb(std::ofstream& out, Color color) {
  const char rgb[3] = {static_cast<char>(color.r()),
                       static_cast<char>(color.g()),
                       static_cast<char>(color.b())};
  out.write(rgb, sizeof(rgb));
}

bool WriteThinOutlineReproPpm(const FakeOffscreen<Argb8888>& actual,
                              const FakeOffscreen<Argb8888>& expected,
                              const std::string& path) {
  const int width = actual.raw_width();
  const int height = actual.raw_height();
  const int gap = kThinOutlineReproGapPixels;
  const int pixel_size = kThinOutlineReproPixelSize;
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) return false;
  out << "P6\n"
      << (width * 3 + gap * 2) * pixel_size << " " << height * pixel_size
      << "\n255\n";

  for (int y = 0; y < height; ++y) {
    for (int py = 0; py < pixel_size; ++py) {
      for (int x = 0; x < width; ++x) {
        Color color = PixelAt(actual, x, y);
        for (int px = 0; px < pixel_size; ++px) {
          WriteRgb(out, color);
        }
      }
      for (int gx = 0; gx < gap * pixel_size; ++gx) {
        WriteRgb(out, color::White);
      }
      for (int x = 0; x < width; ++x) {
        Color color = PixelAt(expected, x, y);
        for (int px = 0; px < pixel_size; ++px) {
          WriteRgb(out, color);
        }
      }
      for (int gx = 0; gx < gap * pixel_size; ++gx) {
        WriteRgb(out, color::White);
      }
      for (int x = 0; x < width; ++x) {
        Color color = PixelAt(actual, x, y) == PixelAt(expected, x, y)
                          ? color::White
                          : color::Magenta;
        for (int px = 0; px < pixel_size; ++px) {
          WriteRgb(out, color);
        }
      }
    }
  }
  return true;
}

}  // namespace

// Verifies ordered centerline bounds normalize to the rounded-inner case.
TEST(SmoothShapes, NormalizeSingleRadiusRoundRectHandlesRoundedInnerCase) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(10.0f, 8.0f, 0.0f, 0.0f, 3.0f,
                                               2.0f);

  EXPECT_EQ(internal::NormalizedRoundRectKind::kRoundInner, normalized.kind);
  EXPECT_FLOAT_EQ(-1.0f, normalized.outer_x0);
  EXPECT_FLOAT_EQ(-1.0f, normalized.outer_y0);
  EXPECT_FLOAT_EQ(11.0f, normalized.outer_x1);
  EXPECT_FLOAT_EQ(9.0f, normalized.outer_y1);
  EXPECT_FLOAT_EQ(4.0f, normalized.outer_radius);
  EXPECT_FLOAT_EQ(1.0f, normalized.inner_x0);
  EXPECT_FLOAT_EQ(1.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(9.0f, normalized.inner_x1);
  EXPECT_FLOAT_EQ(7.0f, normalized.inner_y1);
  EXPECT_FLOAT_EQ(2.0f, normalized.inner_radius);
}

// Verifies normalization clamps radius to half of the centerline minor axis.
TEST(SmoothShapes, NormalizeSingleRadiusRoundRectClampsRadiusToMinorAxis) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(0.0f, 0.0f, 10.0f, 6.0f, 5.0f,
                                               2.0f);

  EXPECT_EQ(internal::NormalizedRoundRectKind::kRoundInner, normalized.kind);
  EXPECT_FLOAT_EQ(4.0f, normalized.outer_radius);
  EXPECT_FLOAT_EQ(2.0f, normalized.inner_radius);
}

// Verifies delta == radius selects the rectangular-inner normalization.
TEST(SmoothShapes,
     NormalizeSingleRadiusRoundRectTurnsThresholdCaseIntoRectInner) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(0.0f, 0.0f, 20.0f, 12.0f, 4.0f,
                                               8.0f);

  EXPECT_EQ(internal::NormalizedRoundRectKind::kRectInner, normalized.kind);
  EXPECT_FLOAT_EQ(4.0f, normalized.inner_x0);
  EXPECT_FLOAT_EQ(4.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(16.0f, normalized.inner_x1);
  EXPECT_FLOAT_EQ(8.0f, normalized.inner_y1);
  EXPECT_FLOAT_EQ(0.0f, normalized.inner_radius);
}

// Verifies the inner rect keeps shrinking after the rounded inner radius hits
// zero.
TEST(SmoothShapes,
     NormalizeSingleRadiusRoundRectKeepsShrinkingRectInnerAfterRadiusZero) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(0.0f, 0.0f, 30.0f, 20.0f, 3.0f,
                                               10.0f);

  EXPECT_EQ(internal::NormalizedRoundRectKind::kRectInner, normalized.kind);
  EXPECT_FLOAT_EQ(5.0f, normalized.inner_x0);
  EXPECT_FLOAT_EQ(5.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(25.0f, normalized.inner_x1);
  EXPECT_FLOAT_EQ(15.0f, normalized.inner_y1);
  EXPECT_FLOAT_EQ(0.0f, normalized.inner_radius);
}

// Verifies collapsed inner bounds normalize to the filled outer-shape case.
TEST(SmoothShapes,
     NormalizeSingleRadiusRoundRectFoldsCollapsedInnerRegionToFilled) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(0.0f, 0.0f, 10.0f, 6.0f, 2.0f,
                                               6.0f);

  EXPECT_EQ(internal::NormalizedRoundRectKind::kFilled, normalized.kind);
  EXPECT_FLOAT_EQ(3.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(3.0f, normalized.inner_y1);
}

TEST(SmoothShapes, DrawSmallCirclesCenterWhole) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 0.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "171"
                             "7F7"
                             "171"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "9F9"
                             "F0F"
                             "9F9"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "04740"
                             "4E7E4"
                             "77077"
                             "4E7E4"
                             "04740"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "2BFB2"
                             "B606B"
                             "F000F"
                             "B606B"
                             "2BFB2"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "0057500"
                             "0AB7BA0"
                             "5B000B5"
                             "7700077"
                             "5B000B5"
                             "0AB7BA0"
                             "0057500"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2, 3}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "06CFC60"
                             "6C303C6"
                             "C30003C"
                             "F00000F"
                             "C30003C"
                             "6C303C6"
                             "06CFC60"));
}

TEST(SmoothShapes, DrawSmallCirclesCenterXoff) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 0.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(2, 2, 3, 4),
                             "55"
                             "FF"
                             "55"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 4, 4),
                             "3DD3"
                             "7777"
                             "3DD3"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 1, 4, 5),
                             "0660"
                             "A99A"
                             "F00F"
                             "A99A"
                             "0660"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 5, 5),
                             "07EE70"
                             "4C11C4"
                             "770077"
                             "4C11C4"
                             "07EE70"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 0, 5, 6),
                             "027720"
                             "4F88F4"
                             "C4004C"
                             "F0000F"
                             "C4004C"
                             "4F88F4"
                             "027720"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 6, 6),
                             "019EE910"
                             "0C7007C0"
                             "5A0000A5"
                             "77000077"
                             "5A0000A5"
                             "0C7007C0"
                             "019EE910"));
}

TEST(SmoothShapes, DrawSmallCirclesCenterXoffYoff) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 0.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(2, 3, 3, 4),
                             "CC"
                             "CC"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 4, 5),
                             "0660"
                             "6AA6"
                             "6AA6"
                             "0660"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 4, 5),
                             "5EE5"
                             "E33E"
                             "E33E"
                             "5EE5"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 5, 6),
                             "016610"
                             "1D88D1"
                             "680086"
                             "680086"
                             "1D88D1"
                             "016610"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 5, 6),
                             "09EE90"
                             "991199"
                             "E1001E"
                             "E1001E"
                             "991199"
                             "09EE90"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothCircle({2.5, 3.5}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 6, 7),
                             "00277200"
                             "07E88E70"
                             "2E1001E2"
                             "78000087"
                             "78000087"
                             "2E1001E2"
                             "07E88E70"
                             "00277200"));
}

TEST(SmoothShapes, FillSmallCirclesCenterWhole) {
  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 1, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "171"
                             "7F7"
                             "171"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 1.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(1, 2, 3, 4),
                             "9F9"
                             "FFF"
                             "9F9"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 2, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "04740"
                             "4FFF4"
                             "7FFF7"
                             "4FFF4"
                             "04740"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 2.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 1, 4, 5),
                             "2BFB2"
                             "BFFFB"
                             "FFFFF"
                             "BFFFB"
                             "2BFB2"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 3, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "0057500"
                             "0AFFFA0"
                             "5FFFFF5"
                             "7FFFFF7"
                             "5FFFFF5"
                             "0AFFFA0"
                             "0057500"));

  EXPECT_THAT(CoercedTo<Alpha4>(SmoothFilledCircle({2, 3}, 3.5, color::Black),
                                Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(-1, 0, 5, 6),
                             "06CFC60"
                             "6FFFFF6"
                             "CFFFFFC"
                             "FFFFFFF"
                             "CFFFFFC"
                             "6FFFFF6"
                             "06CFC60"));
}

TEST(SmoothShapes, DrawTinyShapes) {
  EXPECT_THAT(CoercedTo<Alpha8>(SmoothFilledCircle({2, 3}, 0.5, color::Black),
                                Alpha8(color::Black)),
              // Pi/4 * 245 = hex(c8).
              MatchesContent(Alpha8(color::Black), Box(2, 3, 2, 3), "C8"));

  EXPECT_THAT(
      CoercedTo<Alpha8>(SmoothFilledCircle({2.1, 3.05}, 0.2, color::Black),
                        Alpha8(color::Black)),
      // Pi * (0.2)^2 * 245 = hex(20).
      MatchesContent(Alpha8(color::Black), Box(2, 3, 2, 3), "20"));

  EXPECT_THAT(CoercedTo<Alpha8>(SmoothCircle({2, 3}, 0, color::Black),
                                Alpha8(color::Black)),
              MatchesContent(Alpha8(color::Black), Box(2, 3, 2, 3), "C8"));
}

// Verifies collapsed inner bounds render as the normalized outer filled round
// rect.
TEST(SmoothShapes, ThickRoundRectWithCollapsedInnerRegionFoldsToFilledOuter) {
  Color outline = color::Black;
  Color interior(0xFFF3EFE7);
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(4.0f, 4.0f, 14.0f, 10.0f, 2.0f,
                                               6.0f);
  ASSERT_EQ(internal::NormalizedRoundRectKind::kFilled, normalized.kind);

  const FakeOffscreen<Argb8888> actual =
      CoercedTo<Argb8888>(SmoothThickRoundRect(4.0f, 4.0f, 14.0f, 10.0f, 2.0f,
                                               6.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> expected = CoercedTo<Argb8888>(
      SmoothFilledRoundRect(normalized.outer_x0, normalized.outer_y0,
                            normalized.outer_x1, normalized.outer_y1,
                            normalized.outer_radius, outline),
      Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::Transparent);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies a one-pixel outline renders the expected smooth ring raster for the
// less-common case where the round-rect centerline sits between pixels.
TEST(SmoothShapes, ThickRoundRectWithUnitOutlineRendersExpectedAlpha4Raster) {
  EXPECT_THAT(
      CoercedTo<Alpha4>(SmoothThickRoundRect(0.5f, 0.5f, 4.5f, 4.5f, 1.0f, 1.0f,
                                             color::Black, color::Transparent),
                        Alpha4(color::Black)),
      MatchesContent(Alpha4(color::Black), Box(0, 0, 5, 5),
                     "067760"
                     "6A77A6"
                     "770077"
                     "770077"
                     "6A77A6"
                     "067760"));
}

// Verifies a half-pixel outline on integer-aligned bounds paints end-to-end
// while staying pale instead of becoming fully opaque.
TEST(SmoothShapes, ThickRoundRectWithHalfOutlineRendersPaleAlpha4Raster) {
  EXPECT_THAT(
      CoercedTo<Alpha4>(SmoothThickRoundRect(0.0f, 0.0f, 5.0f, 5.0f, 1.0f, 0.5f,
                                             color::Black, color::Transparent),
                        Alpha4(color::Black)),
      MatchesContent(Alpha4(color::Black), Box(0, 0, 5, 5),
                     "577775"
                     "700007"
                     "700007"
                     "700007"
                     "700007"
                     "577775"));
}

// Verifies zero outline thickness renders the same raster as the equivalent
// filled round rect instead of tinting the outer antialiasing fringe.
TEST(SmoothShapes, ThickRoundRectWithZeroOutlineMatchesFilledRoundRect) {
  Color outline = color::Black;
  Color interior(0x95E07A5F);

  const FakeOffscreen<Argb8888> actual =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.5f, 0.5f, 4.5f, 4.5f, 1.0f,
                                               0.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> expected = CoercedTo<Argb8888>(
      SmoothFilledRoundRect(0.5f, 0.5f, 4.5f, 4.5f, 1.0f, interior), Argb8888(),
      color::Transparent, FillMode::kVisible, BlendingMode::kSourceOver,
      color::Transparent);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies the round-rect draw fast path blends translucent outline-only spans
// against the background rather than against the interior.
TEST(SmoothShapes, ThickRoundRectWithTranslucentOutlineMatchesReadColorsPath) {
  Color outline = color::Black.withA(0x95);
  Color interior(0xFFF3EFE7);
  const SmoothShape shape = SmoothThickRoundRect(0.5f, 0.5f, 16.5f, 16.5f, 3.0f,
                                                 2.0f, outline, interior);
  const ForcedRasterizable reference(&shape);

  const FakeOffscreen<Argb8888> actual = CoercedTo<Argb8888>(
      shape, Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> expected = CoercedTo<Argb8888>(
      reference, Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::Transparent);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies overlarge radii render the same as the half-width clamp value.
TEST(SmoothShapes, ThickRoundRectClampsRadiusBeforeRendering) {
  Color outline = color::Black;
  Color interior(0xFFF3EFE7);

  const FakeOffscreen<Argb8888> actual =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.5f, 0.5f, 4.5f, 8.5f, 5.0f,
                                               1.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> expected =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.5f, 0.5f, 4.5f, 8.5f, 2.0f,
                                               1.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies the rectangular-inner mode keeps shrinking the interior once delta
// exceeds the centerline radius.
TEST(SmoothShapes, ThickRoundRectWithRectInnerShrinksInteriorBounds) {
  Color outline = color::Black;
  Color interior(0xFFF3EFE7);
  const SmoothShape shape = SmoothThickRoundRect(0.5f, 0.5f, 16.5f, 16.5f, 1.0f,
                                                 3.0f, outline, interior);
  const FakeOffscreen<Argb8888> rendered = CoercedTo<Argb8888>(
      shape, Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::Transparent);
  const Color edge_color(0xFF000000 | ((interior.r() + 1) / 2) << 16 |
                         ((interior.g() + 1) / 2) << 8 |
                         ((interior.b() + 1) / 2));

  EXPECT_EQ(interior, PixelAt(rendered, 4, 9));
  EXPECT_EQ(interior, PixelAt(rendered, 9, 4));
  EXPECT_EQ(edge_color, PixelAt(rendered, 3, 9));
  EXPECT_EQ(edge_color, PixelAt(rendered, 9, 3));
}

// Verifies thickness == 2 * radius keeps the rectangular inner region instead
// of collapsing to a filled outer round rect.
TEST(SmoothShapes,
     ThickRoundRectWithDoubleRadiusOutlineKeepsInteriorDistinctFromFilled) {
  Color outline = color::Black;
  Color interior(0xFFF3EFE7);
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(0.5f, 0.5f, 16.5f, 16.5f, 1.0f,
                                               2.0f);
  ASSERT_EQ(internal::NormalizedRoundRectKind::kRectInner, normalized.kind);

  const FakeOffscreen<Argb8888> rendered =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.5f, 0.5f, 16.5f, 16.5f, 1.0f,
                                               2.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> filled = CoercedTo<Argb8888>(
      SmoothFilledRoundRect(normalized.outer_x0, normalized.outer_y0,
                            normalized.outer_x1, normalized.outer_y1,
                            normalized.outer_radius, outline),
      Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::Transparent);

  EXPECT_EQ(interior, PixelAt(rendered, 3, 3));
  EXPECT_EQ(interior, PixelAt(rendered, 3, 9));
  EXPECT_EQ(interior, PixelAt(rendered, 9, 3));
  EXPECT_EQ(outline, PixelAt(filled, 3, 3));
  EXPECT_NE(PixelAt(filled, 3, 3), PixelAt(rendered, 3, 3));
}

// Verifies the half-width round-rect path stays close to a supersampled
// reference for the worst translucent-outline / opaque-interior contrast case.
TEST(SmoothShapes,
     ThickRoundRectWithTranslucentThinOutlineTracksSupersampledReference) {
  const FakeOffscreen<Argb8888> actual = RenderThinOutlineRepro(
      kAlphaContrastReproThickness, kAlphaContrastOutlineOpaque.withA(0x40),
      kAlphaContrastInteriorOpaque, 1);
  const FakeOffscreen<Argb8888> supersampled = RenderThinOutlineRepro(
      kAlphaContrastReproThickness, kAlphaContrastOutlineOpaque.withA(0x40),
      kAlphaContrastInteriorOpaque, kThinOutlineReproScale);
  const FakeOffscreen<Argb8888> expected = DownsampleByAveraging(
      supersampled, supersampled.raw_width() / actual.raw_width());

  EXPECT_LT(NormalizedRmse(actual, expected), 0.04f);
}

// Verifies the thin half-width round-rect path returns the expected raw ARGB
// on a transparent target across the outline/interior alpha matrix.
TEST(SmoothShapes,
     ThickRoundRectWithThinOutlineTracksTransparentSupersampledReference) {
  for (const ThinOutlineAlphaContrastReproCase& repro_case :
       kThinOutlineAlphaContrastReproCases) {
    SCOPED_TRACE(repro_case.basename);
    const FakeOffscreen<Argb8888> actual = RenderThinOutlineRepro(
        kAlphaContrastReproThickness, repro_case.outline, repro_case.interior,
        1, color::Transparent);
    const FakeOffscreen<Argb8888> supersampled = RenderThinOutlineRepro(
        kAlphaContrastReproThickness, repro_case.outline, repro_case.interior,
        kThinOutlineReproScale, color::Transparent);

    ASSERT_EQ(0, supersampled.raw_width() % actual.raw_width());
    ASSERT_EQ(0, supersampled.raw_height() % actual.raw_height());
    ASSERT_EQ(supersampled.raw_width() / actual.raw_width(),
              supersampled.raw_height() / actual.raw_height());

    const int factor = supersampled.raw_width() / actual.raw_width();
    const FakeOffscreen<Argb8888> expected =
        DownsampleByPremultipliedAveraging(supersampled, factor);

    EXPECT_LT(NormalizedRmse(actual, expected), 0.04f);
  }
}

// Generates manual-inspection repro images comparing the current thin-outline
// raster against a supersampled reference.
TEST(SmoothShapes, DISABLED_GenerateThinOutlineRoundRectRepros) {
  for (const ThinOutlineReproCase& repro_case : kThinOutlineReproCases) {
    SCOPED_TRACE(repro_case.basename);
    const FakeOffscreen<Argb8888> actual =
        RenderThinOutlineRepro(repro_case.thickness, repro_case.outline, 1);
    const FakeOffscreen<Argb8888> supersampled = RenderThinOutlineRepro(
        repro_case.thickness, repro_case.outline, kThinOutlineReproScale);

    ASSERT_EQ(0, supersampled.raw_width() % actual.raw_width());
    ASSERT_EQ(0, supersampled.raw_height() % actual.raw_height());
    ASSERT_EQ(supersampled.raw_width() / actual.raw_width(),
              supersampled.raw_height() / actual.raw_height());

    const int factor = supersampled.raw_width() / actual.raw_width();
    const FakeOffscreen<Argb8888> expected =
        DownsampleByAveraging(supersampled, factor);
    const std::string ppm_path =
        std::string("test/") + repro_case.basename + ".ppm";

    ASSERT_TRUE(WriteThinOutlineReproPpm(actual, expected, ppm_path));
    std::cout << "Wrote " << ppm_path << "\n";
  }
}

// Generates manual-inspection repro images for contrasting outline/interior
// colors across the opaque/translucent alpha matrix.
TEST(SmoothShapes, DISABLED_GenerateThinOutlineAlphaContrastRepros) {
  for (const ThinOutlineAlphaContrastReproCase& repro_case :
       kThinOutlineAlphaContrastReproCases) {
    SCOPED_TRACE(repro_case.basename);
    const FakeOffscreen<Argb8888> actual =
        RenderThinOutlineRepro(kAlphaContrastReproThickness, repro_case.outline,
                               repro_case.interior, 1);
    const FakeOffscreen<Argb8888> supersampled =
        RenderThinOutlineRepro(kAlphaContrastReproThickness, repro_case.outline,
                               repro_case.interior, kThinOutlineReproScale);

    ASSERT_EQ(0, supersampled.raw_width() % actual.raw_width());
    ASSERT_EQ(0, supersampled.raw_height() % actual.raw_height());
    ASSERT_EQ(supersampled.raw_width() / actual.raw_width(),
              supersampled.raw_height() / actual.raw_height());

    const int factor = supersampled.raw_width() / actual.raw_width();
    const FakeOffscreen<Argb8888> expected =
        DownsampleByAveraging(supersampled, factor);
    const std::string ppm_path =
        std::string("test/") + repro_case.basename + ".ppm";

    ASSERT_TRUE(WriteThinOutlineReproPpm(actual, expected, ppm_path));
    std::cout << "Wrote " << ppm_path << "\n";
  }
}

}  // namespace roo_display

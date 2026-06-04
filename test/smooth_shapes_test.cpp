
#include <math.h>

#include <array>
#include <fstream>
#include <string>

#include "roo_display/color/color.h"
#include "roo_display/shape/impl/smooth_internal.h"
#include "roo_display/shape/smooth.h"
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
  const char* expected_raster;
};

struct RoundRectStreamParityCase {
  const char* basename;
  float x0;
  float y0;
  float x1;
  float y1;
  float radius;
  float thickness;
  Color outline;
  Color interior;
  Color background;
  internal::NormalizedRoundRectKind expected_kind;
  bool expect_inner_shrunk_below_center_rect;
};

struct RoundRectCornersStreamParityCase {
  const char* basename;
  float x0;
  float y0;
  float x1;
  float y1;
  RoundRectRadii radii;
  float thickness;
  Color outline;
  Color interior;
  Color background;
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
         "smooth_roundrect_thin_outline_050_outline100_interior100_repro",
         "00000000 2BFF3B30 3FFF3B30 3FFF3B30 2BFF3B30 00000000"
         "2BFF3B30 FF75878C FF40A4AF FF40A4AF FF75878C 2BFF3B30"
         "3FFF3B30 FF40A4AF FF00C7D9 FF00C7D9 FF40A4AF 3FFF3B30"
         "3FFF3B30 FF40A4AF FF00C7D9 FF00C7D9 FF40A4AF 3FFF3B30"
         "2BFF3B30 FF75878C FF40A4AF FF40A4AF FF75878C 2BFF3B30"
         "00000000 2BFF3B30 3FFF3B30 3FFF3B30 2BFF3B30 00000000"},
        {kAlphaContrastOutlineOpaque, kAlphaContrastInteriorOpaque.withA(0x40),
         "smooth_roundrect_thin_outline_050_outline100_interior025_repro",
         "00000000 2BFF3B30 3FFF3B30 3FFF3B30 2BFF3B30 00000000"
         "2BFF3B30 97C45B57 6F917779 6F917779 97C45B57 2BFF3B30"
         "3FFF3B30 6F917779 4000C7D9 4000C7D9 6F917779 3FFF3B30"
         "3FFF3B30 6F917779 4000C7D9 4000C7D9 6F917779 3FFF3B30"
         "2BFF3B30 97C45B57 6F917779 6F917779 97C45B57 2BFF3B30"
         "00000000 2BFF3B30 3FFF3B30 3FFF3B30 2BFF3B30 00000000"},
        {kAlphaContrastOutlineOpaque.withA(0x40), kAlphaContrastInteriorOpaque,
         "smooth_roundrect_thin_outline_050_outline025_interior100_repro",
         "00000000 0AFF3B30 10FF3B30 10FF3B30 0AFF3B30 00000000"
         "0AFF3B30 A72CAFBC CF13BDCC CF13BDCC A72CAFBC 0AFF3B30"
         "10FF3B30 CF13BDCC FF00C7D9 FF00C7D9 CF13BDCC 10FF3B30"
         "10FF3B30 CF13BDCC FF00C7D9 FF00C7D9 CF13BDCC 10FF3B30"
         "0AFF3B30 A72CAFBC CF13BDCC CF13BDCC A72CAFBC 0AFF3B30"
         "00000000 0AFF3B30 10FF3B30 10FF3B30 0AFF3B30 00000000"},
        {kAlphaContrastOutlineOpaque.withA(0x40),
         kAlphaContrastInteriorOpaque.withA(0x40),
         "smooth_roundrect_thin_outline_050_outline025_interior025_repro",
         "00000000 0AFF3B30 10FF3B30 10FF3B30 0AFF3B30 00000000"
         "0AFF3B30 4075878C 4040A4AF 4040A4AF 4075878C 0AFF3B30"
         "10FF3B30 4040A4AF 4000C7D9 4000C7D9 4040A4AF 10FF3B30"
         "10FF3B30 4040A4AF 4000C7D9 4000C7D9 4040A4AF 10FF3B30"
         "0AFF3B30 4075878C 4040A4AF 4040A4AF 4075878C 0AFF3B30"
         "00000000 0AFF3B30 10FF3B30 10FF3B30 0AFF3B30 00000000"},
    }};

const std::array<RoundRectStreamParityCase, 5> kRoundRectStreamParityCases = {{
    {"rounded_inner_large_opaque", 0.5f, 0.5f, 32.5f, 24.5f, 4.0f, 2.0f,
     color::Black, Color(0xFFF3EFE7), color::Transparent,
     internal::NormalizedRoundRectKind::kRoundInner, false},
    {"rounded_inner_translucent_on_opaque_bg", 0.5f, 0.5f, 16.5f, 16.5f, 3.0f,
     2.0f, color::Black.withA(0x95), Color(0x80F3EFE7), color::White,
     internal::NormalizedRoundRectKind::kRoundInner, false},
    {"filled_fold_outline_only", 4.0f, 4.0f, 14.0f, 10.0f, 2.0f, 6.0f,
     color::Black, Color(0xFFF3EFE7), color::Transparent,
     internal::NormalizedRoundRectKind::kFilled, false},
    {"rect_inner_medium", 0.5f, 0.5f, 16.5f, 16.5f, 1.0f, 3.0f, color::Black,
     Color(0xFFF3EFE7), color::Transparent,
     internal::NormalizedRoundRectKind::kRectInner, false},
    {"rect_inner_very_thick", 2.5f, 2.5f, 12.5f, 12.5f, 2.0f, 5.0f,
     color::Black, color::Transparent, color::Transparent,
     internal::NormalizedRoundRectKind::kRectInner, true},
}};

const std::array<RoundRectCornersStreamParityCase, 4>
    kRoundRectCornersStreamParityCases = {{
        {"outlined_asymmetric", 0.5f, 0.5f, 24.5f, 18.5f,
         RoundRectRadii{5.0f, 2.0f, 4.0f, 3.0f}, 2.0f, color::Black.withA(0x95),
         Color(0x80F3EFE7), color::White},
        {"collapsed_inner_tr_right", 0.0f, 0.0f, 18.0f, 12.0f,
         RoundRectRadii{3.0f, 1.0f, 3.0f, 1.0f}, 2.0f, color::Black,
         Color(0xFFF3EFE7), color::Transparent},
        {"filled_asymmetric", 0.5f, 0.5f, 20.5f, 14.5f,
         RoundRectRadii{4.0f, 1.5f, 3.0f, 2.5f}, 0.0f, Color(0xFF4A7C59),
         Color(0xFF4A7C59), color::Transparent},
        {"translucent_outline_interior", 1.25f, 1.75f, 17.75f, 13.25f,
         RoundRectRadii{4.0f, 2.5f, 1.5f, 3.5f}, 1.5f, color::Black.withA(0x95),
         Color(0x40F3EFE7), color::White},
    }};

Color PixelAt(const FakeOffscreen<Argb8888>& offscreen, int16_t x, int16_t y) {
  return offscreen.buffer()[y * offscreen.raw_width() + x];
}

FakeOffscreen<Argb8888> RenderThinOutlineRepro(
    float thickness, Color outline, Color interior, int scale,
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

template <typename ColorMode>
FakeOffscreen<ColorMode> CoercedToViaStreaming(
    const Streamable& streamable, ColorMode color_mode = ColorMode(),
    Color fill = color::Transparent, FillMode fill_mode = FillMode::kVisible,
    BlendingMode blending_mode = BlendingMode::kSourceOver,
    Color bgcolor = color::Transparent) {
  return CoercedTo<ColorMode>(ForcedStreamable(&streamable), color_mode, fill,
                              fill_mode, blending_mode, bgcolor);
}

template <typename ColorMode>
FakeOffscreen<ColorMode> CoercedToClipped(
    const Drawable& drawable, const Box& clip_box,
    ColorMode color_mode = ColorMode(), Color fill = color::Transparent,
    FillMode fill_mode = FillMode::kVisible,
    BlendingMode blending_mode = BlendingMode::kSourceOver,
    Color bgcolor = color::Transparent) {
  FakeOffscreen<ColorMode> result(drawable.extents(), fill, color_mode);
  result.begin();
  Box extents = drawable.extents();
  Box translated_clip =
      Box::Intersect(clip_box.translate(-extents.xMin(), -extents.yMin()),
                     Box(0, 0, extents.width() - 1, extents.height() - 1));
  Surface s(result, -extents.xMin(), -extents.yMin(), translated_clip, false,
            bgcolor, fill_mode, blending_mode);
  s.drawObject(drawable);
  result.end();
  return result;
}

FakeOffscreen<Argb8888> RenderShapeArgb8888(const Drawable& drawable) {
  return CoercedTo<Argb8888>(drawable, Argb8888(), color::Transparent,
                             FillMode::kVisible, BlendingMode::kSourceOver,
                             color::Transparent);
}

FakeOffscreen<Argb8888> StreamShapeArgb8888(const Streamable& streamable) {
  return CoercedToViaStreaming<Argb8888>(
      streamable, Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::Transparent);
}

void ExpectRunLengthTracksExactSolidSegmentRemainder(const SmoothShape& shape) {
  std::unique_ptr<PixelStream> stream = shape.createStream();
  const Box bounds = shape.extents();
  const uint32_t pixel_count = (uint32_t)bounds.width() * bounds.height();

  bool saw_nonzero_run = false;
  bool saw_zero_run = false;
  Color prev_color = color::Transparent;
  uint32_t prev_run_length = 0;
  bool has_prev = false;

  for (uint32_t i = 0; i < pixel_count; ++i) {
    Color current;
    uint32_t run_length = 0;
    stream->read(&current, 1, run_length);

    if (run_length == 0) {
      saw_zero_run = true;
    } else {
      saw_nonzero_run = true;
    }

    if (has_prev && prev_run_length > 1) {
      EXPECT_EQ(prev_color, current);
      EXPECT_EQ(prev_run_length - 1, run_length);
    }

    prev_color = current;
    prev_run_length = run_length;
    has_prev = true;
  }

  EXPECT_TRUE(saw_nonzero_run);
  EXPECT_TRUE(saw_zero_run);
}

void ExpectTranslatedShapeMatches(const SmoothShape& original,
                                  const SmoothShape& expected, int16_t dx,
                                  int16_t dy,
                                  bool expect_stream_match = false) {
  const SmoothShape translated = original.translate(dx, dy);

  EXPECT_EQ(original.extents().translate(dx, dy), translated.extents());
  EXPECT_EQ(expected.extents(), translated.extents());

  const FakeOffscreen<Argb8888> expected_draw = RenderShapeArgb8888(expected);
  const FakeOffscreen<Argb8888> actual_draw = RenderShapeArgb8888(translated);
  EXPECT_THAT(RasterOf(actual_draw), MatchesContent(RasterOf(expected_draw)));

  if (expect_stream_match) {
    const FakeOffscreen<Argb8888> actual_stream =
        StreamShapeArgb8888(translated);
    EXPECT_THAT(RasterOf(actual_stream),
                MatchesContent(RasterOf(expected_draw)));
  }
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

// Verifies thickness greater than twice the radius folds collapsed inner
// bounds to the filled outer-shape case.
TEST(SmoothShapes,
     NormalizeSingleRadiusRoundRectFoldsCollapsedInnerRegionToFilled) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(0.0f, 0.0f, 10.0f, 6.0f, 2.0f,
                                               6.0f);

  EXPECT_EQ(internal::NormalizedRoundRectKind::kFilled, normalized.kind);
  EXPECT_FLOAT_EQ(3.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(3.0f, normalized.inner_y1);
}

// Verifies equal effective corner radii normalize to the same geometry as the
// single-radius helper.
TEST(SmoothShapes, NormalizeFourRadiiRoundRectMatchesSingleRadiusWhenEqual) {
  const RoundRectRadii radii{3.0f, 3.0f, 3.0f, 3.0f};
  const internal::NormalizedFourRadiiRoundRect normalized =
      internal::NormalizeFourRadiiRoundRect(10.0f, 8.0f, 0.0f, 0.0f, radii,
                                            2.0f);
  const internal::NormalizedSingleRadiusRoundRect single =
      internal::NormalizeSingleRadiusRoundRect(10.0f, 8.0f, 0.0f, 0.0f, 3.0f,
                                               2.0f);

  EXPECT_FALSE(normalized.filled);
  EXPECT_TRUE(normalized.has_equal_centerline_radii);
  EXPECT_FLOAT_EQ(0.0f, normalized.centerline_x0);
  EXPECT_FLOAT_EQ(0.0f, normalized.centerline_y0);
  EXPECT_FLOAT_EQ(10.0f, normalized.centerline_x1);
  EXPECT_FLOAT_EQ(8.0f, normalized.centerline_y1);
  EXPECT_FLOAT_EQ(single.outer_x0, normalized.outer_x0);
  EXPECT_FLOAT_EQ(single.outer_y0, normalized.outer_y0);
  EXPECT_FLOAT_EQ(single.outer_x1, normalized.outer_x1);
  EXPECT_FLOAT_EQ(single.outer_y1, normalized.outer_y1);
  EXPECT_FLOAT_EQ(single.inner_x0, normalized.inner_x0);
  EXPECT_FLOAT_EQ(single.inner_y0, normalized.inner_y0);
  EXPECT_FLOAT_EQ(single.inner_x1, normalized.inner_x1);
  EXPECT_FLOAT_EQ(single.inner_y1, normalized.inner_y1);
  EXPECT_FLOAT_EQ(3.0f, normalized.centerline_radii.tl);
  EXPECT_FLOAT_EQ(3.0f, normalized.centerline_radii.br);
  EXPECT_FLOAT_EQ(4.0f, normalized.outer_radii.tl);
  EXPECT_FLOAT_EQ(4.0f, normalized.outer_radii.br);
  EXPECT_FLOAT_EQ(2.0f, normalized.inner_radii.tl);
  EXPECT_FLOAT_EQ(2.0f, normalized.inner_radii.br);
}

// Verifies one global scale factor is applied to all corner radii before the
// thick outer and inner radii are derived.
TEST(SmoothShapes, NormalizeFourRadiiRoundRectGloballyScalesRadiiToFitBounds) {
  const RoundRectRadii radii{8.0f, 4.0f, 8.0f, 4.0f};
  const internal::NormalizedFourRadiiRoundRect normalized =
      internal::NormalizeFourRadiiRoundRect(0.0f, 0.0f, 10.0f, 10.0f, radii,
                                            2.0f);

  EXPECT_FALSE(normalized.filled);
  EXPECT_FALSE(normalized.has_equal_centerline_radii);
  EXPECT_FLOAT_EQ(-1.0f, normalized.outer_x0);
  EXPECT_FLOAT_EQ(-1.0f, normalized.outer_y0);
  EXPECT_FLOAT_EQ(11.0f, normalized.outer_x1);
  EXPECT_FLOAT_EQ(11.0f, normalized.outer_y1);
  EXPECT_FLOAT_EQ(1.0f, normalized.inner_x0);
  EXPECT_FLOAT_EQ(1.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(9.0f, normalized.inner_x1);
  EXPECT_FLOAT_EQ(9.0f, normalized.inner_y1);
  EXPECT_FLOAT_EQ(5.0f, normalized.centerline_radii.tl);
  EXPECT_FLOAT_EQ(2.5f, normalized.centerline_radii.tr);
  EXPECT_FLOAT_EQ(5.0f, normalized.centerline_radii.bl);
  EXPECT_FLOAT_EQ(2.5f, normalized.centerline_radii.br);
  EXPECT_FLOAT_EQ(6.0f, normalized.outer_radii.tl);
  EXPECT_FLOAT_EQ(3.5f, normalized.outer_radii.tr);
  EXPECT_FLOAT_EQ(4.0f, normalized.inner_radii.tl);
  EXPECT_FLOAT_EQ(1.5f, normalized.inner_radii.tr);
}

// Verifies the four-radii helper reports the filled fold as soon as the
// corrected inner bounds collapse in either axis.
TEST(SmoothShapes, NormalizeFourRadiiRoundRectDetectsFilledFold) {
  const RoundRectRadii radii{2.0f, 1.0f, 2.0f, 1.0f};
  const internal::NormalizedFourRadiiRoundRect normalized =
      internal::NormalizeFourRadiiRoundRect(0.0f, 0.0f, 10.0f, 6.0f, radii,
                                            6.0f);

  EXPECT_TRUE(normalized.filled);
  EXPECT_FALSE(normalized.has_equal_centerline_radii);
  EXPECT_FLOAT_EQ(3.0f, normalized.inner_y0);
  EXPECT_FLOAT_EQ(3.0f, normalized.inner_y1);
}

// Verifies the per-corner API delegates equal effective radii to the corrected
// single-radius renderer.
TEST(SmoothShapes,
     ThickRoundRectCornerRadiiMatchesSingleRadiusRasterWhenEqual) {
  const RoundRectRadii radii{3.0f, 3.0f, 3.0f, 3.0f};
  const Color outline = color::Black.withA(0x95);
  const Color interior(0xFFF3EFE7);

  const FakeOffscreen<Argb8888> actual =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.5f, 0.5f, 16.5f, 12.5f, radii,
                                               2.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> expected =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.5f, 0.5f, 16.5f, 12.5f, 3.0f,
                                               2.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies raw unequal radii that clamp to the same effective value still fold
// into the corrected single-radius path.
TEST(SmoothShapes,
     ThickRoundRectCornerRadiiMatchesSingleRadiusAfterClampToEqual) {
  const RoundRectRadii radii{-1.0f, 0.0f, 0.0f, 0.0f};
  const Color outline = color::Black.withA(0x95);
  const Color interior(0xFFF3EFE7);

  const FakeOffscreen<Argb8888> actual =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.0f, 0.0f, 12.0f, 8.0f, radii,
                                               2.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);
  const FakeOffscreen<Argb8888> expected =
      CoercedTo<Argb8888>(SmoothThickRoundRect(0.0f, 0.0f, 12.0f, 8.0f, 0.0f,
                                               2.0f, outline, interior),
                          Argb8888(), color::Transparent, FillMode::kVisible,
                          BlendingMode::kSourceOver, color::Transparent);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies genuinely unequal radii sample transparent, straight-edge AA,
// outline, and interior pixels correctly through the unequal-radius evaluator.
TEST(SmoothShapes, ThickRoundRectCornerRadiiReadsUnequalPayloadColors) {
  const Color outline = color::Black;
  const Color interior(0xFFF3EFE7);
  const SmoothShape shape = SmoothThickRoundRect(
      0.0f, 0.0f, 12.0f, 10.0f, RoundRectRadii{4.0f, 2.0f, 4.0f, 2.0f}, 2.0f,
      outline, interior);

  ASSERT_FALSE(shape.extents().empty());
  Color sample;
  EXPECT_TRUE(shape.readColorRect(-1, -1, -1, -1, &sample));
  EXPECT_EQ(color::Transparent, sample);
  EXPECT_TRUE(shape.readColorRect(6, -1, 6, -1, &sample));
  EXPECT_EQ(outline.withA(0x7F), sample);
  EXPECT_TRUE(shape.readColorRect(6, 0, 6, 0, &sample));
  EXPECT_EQ(outline, sample);
  EXPECT_TRUE(shape.readColorRect(6, 5, 6, 5, &sample));
  EXPECT_EQ(interior, sample);
}

// Verifies unequal-radius readback reuses the dedicated rectangle classifier
// for uniform outline tiles and still reports mixed AA tiles as non-uniform.
TEST(SmoothShapes, ThickRoundRectCornerRadiiReadColorRectUsesClassifier) {
  const Color outline = color::Black;
  const Color interior(0xFFF3EFE7);
  const SmoothShape shape = SmoothThickRoundRect(
      0.0f, 0.0f, 12.0f, 10.0f, RoundRectRadii{4.0f, 2.0f, 4.0f, 2.0f}, 2.0f,
      outline, interior);

  Color sample[4];
  EXPECT_TRUE(shape.readColorRect(5, 0, 7, 0, sample));
  EXPECT_EQ(outline, sample[0]);

  Color uniform_sample;
  EXPECT_FALSE(shape.readUniformColorRect(-1, -1, 1, 1, &uniform_sample));
}

// Verifies the dedicated unequal-radius draw path matches the streaming path,
// including the clipped center-fill partition and tiled perimeter bands.
TEST(SmoothShapes, ThickRoundRectCornerRadiiClippedDrawMatchesStreaming) {
  const Color outline = color::Black.withA(0x95);
  const Color interior(0x80F3EFE7);
  const SmoothShape shape = SmoothThickRoundRect(
      0.0f, 0.0f, 20.0f, 14.0f, RoundRectRadii{5.0f, 2.0f, 4.0f, 3.0f}, 2.0f,
      outline, interior);
  const Box clip(shape.extents().xMin() + 1, shape.extents().yMin() + 2,
                 shape.extents().xMax() - 2, shape.extents().yMax() - 1);
  const ForcedStreamable forced(&shape);

  const FakeOffscreen<Argb8888> actual = CoercedToClipped<Argb8888>(
      shape, clip, Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::White);
  const FakeOffscreen<Argb8888> expected = CoercedToClipped<Argb8888>(
      forced, clip, Argb8888(), color::Transparent, FillMode::kVisible,
      BlendingMode::kSourceOver, color::White);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

TEST(SmoothShapes, RoundRectStreamReportsExactSolidSegmentPrefixRuns) {
  const SmoothShape shape = SmoothThickRoundRect(
      0.5f, 0.5f, 18.5f, 14.5f, 4.0f, 2.0f, color::Black, Color(0xFFF3EFE7));

  ExpectRunLengthTracksExactSolidSegmentRemainder(shape);
}

TEST(SmoothShapes, RectInnerRoundRectStreamReportsExactSolidSegmentPrefixRuns) {
  const SmoothShape shape = SmoothThickRoundRect(
      0.5f, 0.5f, 20.5f, 14.5f, 2.0f, 5.0f, color::Black, Color(0xFFF3EFE7));

  ExpectRunLengthTracksExactSolidSegmentRemainder(shape);
}

TEST(SmoothShapes,
     UnequalCornerRoundRectStreamReportsExactSolidSegmentPrefixRuns) {
  const SmoothShape shape = SmoothThickRoundRect(
      0.5f, 0.5f, 20.5f, 14.5f, RoundRectRadii{5.0f, 2.0f, 4.0f, 3.0f}, 2.0f,
      color::Black.withA(0x95), Color(0x80F3EFE7));

  ExpectRunLengthTracksExactSolidSegmentRemainder(shape);
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

TEST(SmoothShapes, TranslateEmptyPreservesEmptyShape) {
  const SmoothShape empty;
  const SmoothShape translated = empty.translate(7, -5);

  EXPECT_EQ(empty.extents(), translated.extents());

  Color sample = color::Black;
  EXPECT_TRUE(translated.readColorRect(10, 10, 10, 10, &sample));
  EXPECT_EQ(color::Transparent, sample);
}

TEST(SmoothShapes, TranslateMatchesDirectFactoriesAcrossKinds) {
  constexpr int16_t dx = 7;
  constexpr int16_t dy = -4;

  ExpectTranslatedShapeMatches(
      SmoothWedgedLine({1.25f, 2.75f}, 3.0f, {10.5f, 8.25f}, 5.0f,
                       Color(0xFF336699), ENDING_FLAT),
      SmoothWedgedLine({1.25f + dx, 2.75f + dy}, 3.0f, {10.5f + dx, 8.25f + dy},
                       5.0f, Color(0xFF336699), ENDING_FLAT),
      dx, dy);

  ExpectTranslatedShapeMatches(
      SmoothThickRoundRect(0.5f, 0.5f, 16.5f, 12.5f, 3.0f, 2.0f,
                           color::Black.withA(0xD0), Color(0xFFF3EFE7)),
      SmoothThickRoundRect(0.5f + dx, 0.5f + dy, 16.5f + dx, 12.5f + dy, 3.0f,
                           2.0f, color::Black.withA(0xD0), Color(0xFFF3EFE7)),
      dx, dy, true);

  ExpectTranslatedShapeMatches(
      SmoothThickRoundRect(1.25f, 1.75f, 17.75f, 13.25f,
                           RoundRectRadii{4.0f, 2.5f, 1.5f, 3.5f}, 1.5f,
                           color::Black.withA(0x95), Color(0x40F3EFE7)),
      SmoothThickRoundRect(1.25f + dx, 1.75f + dy, 17.75f + dx, 13.25f + dy,
                           RoundRectRadii{4.0f, 2.5f, 1.5f, 3.5f}, 1.5f,
                           color::Black.withA(0x95), Color(0x40F3EFE7)),
      dx, dy, true);

  ExpectTranslatedShapeMatches(
      SmoothThickArcWithBackground({9.0f, 8.0f}, 6.0f, 2.5f, -0.9f, 1.8f,
                                   Color(0xFF3B82F6), Color(0xFF94A3B8),
                                   Color(0x804B5563), ENDING_ROUNDED),
      SmoothThickArcWithBackground({9.0f + dx, 8.0f + dy}, 6.0f, 2.5f, -0.9f,
                                   1.8f, Color(0xFF3B82F6), Color(0xFF94A3B8),
                                   Color(0x804B5563), ENDING_ROUNDED),
      dx, dy);

  ExpectTranslatedShapeMatches(
      SmoothFilledTriangle({0.5f, 1.0f}, {8.5f, 2.5f}, {3.0f, 10.0f},
                           Color(0xFFC2410C)),
      SmoothFilledTriangle({0.5f + dx, 1.0f + dy}, {8.5f + dx, 2.5f + dy},
                           {3.0f + dx, 10.0f + dy}, Color(0xFFC2410C)),
      dx, dy);

  ExpectTranslatedShapeMatches(
      SmoothCircle({2.0f, 3.0f}, 0.0f, color::Black),
      SmoothCircle({2.0f + dx, 3.0f + dy}, 0.0f, color::Black), dx, dy);
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

// Verifies the round-rect streaming path matches the regular draw path across
// rounded-inner, filled-fold, and rectangular-inner geometry, including the
// very-thick rectangular-inner case where the inner rect shrinks past the
// frozen center rect.
TEST(SmoothShapes,
     ThickRoundRectStreamingMatchesRegularPathAcrossInterestingCases) {
  for (const RoundRectStreamParityCase& test_case :
       kRoundRectStreamParityCases) {
    SCOPED_TRACE(test_case.basename);
    const internal::NormalizedSingleRadiusRoundRect normalized =
        internal::NormalizeSingleRadiusRoundRect(
            test_case.x0, test_case.y0, test_case.x1, test_case.y1,
            test_case.radius, test_case.thickness);
    ASSERT_EQ(test_case.expected_kind, normalized.kind);
    if (test_case.expect_inner_shrunk_below_center_rect) {
      float left = test_case.x0 < test_case.x1 ? test_case.x0 : test_case.x1;
      float right = test_case.x0 < test_case.x1 ? test_case.x1 : test_case.x0;
      float top = test_case.y0 < test_case.y1 ? test_case.y0 : test_case.y1;
      float bottom = test_case.y0 < test_case.y1 ? test_case.y1 : test_case.y0;
      float width = right - left;
      float height = bottom - top;
      float clamped_radius = test_case.radius;
      float max_radius = ((width < height) ? width : height) * 0.5f;
      if (clamped_radius > max_radius) clamped_radius = max_radius;

      EXPECT_GT(normalized.inner_x0, left + clamped_radius);
      EXPECT_GT(normalized.inner_y0, top + clamped_radius);
      EXPECT_LT(normalized.inner_x1, right - clamped_radius);
      EXPECT_LT(normalized.inner_y1, bottom - clamped_radius);
    }

    const SmoothShape shape = SmoothThickRoundRect(
        test_case.x0, test_case.y0, test_case.x1, test_case.y1,
        test_case.radius, test_case.thickness, test_case.outline,
        test_case.interior);
    const FakeOffscreen<Argb8888> actual = CoercedToViaStreaming(
        shape, Argb8888(), test_case.background, FillMode::kVisible,
        BlendingMode::kSourceOver, test_case.background);
    const FakeOffscreen<Argb8888> expected = CoercedTo<Argb8888>(
        shape, Argb8888(), test_case.background, FillMode::kVisible,
        BlendingMode::kSourceOver, test_case.background);

    EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
  }
}

// Verifies the dedicated rectangular-inner stream matches the readColors path
// when clipped to a sub-rectangle that begins and ends mid-row.
TEST(SmoothShapes,
     ThickRoundRectRectInnerStreamingMatchesRegularPathWhenClipped) {
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(1.25f, 1.75f, 17.75f, 13.25f,
                                               1.25f, 3.0f);
  ASSERT_EQ(internal::NormalizedRoundRectKind::kRectInner, normalized.kind);

  const SmoothShape shape =
      SmoothThickRoundRect(1.25f, 1.75f, 17.75f, 13.25f, 1.25f, 3.0f,
                           color::Black.withA(0x95), Color(0x80F3EFE7));
  const Box clip_box(3, 3, 16, 12);

  const FakeOffscreen<Argb8888> actual = CoercedToClipped(
      ForcedStreamable(&shape), clip_box, Argb8888(), color::Transparent,
      FillMode::kVisible, BlendingMode::kSourceOver, color::White);
  const FakeOffscreen<Argb8888> expected = CoercedToClipped(
      ForcedRasterizable(&shape), clip_box, Argb8888(), color::Transparent,
      FillMode::kVisible, BlendingMode::kSourceOver, color::White);

  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

// Verifies the dedicated unequal-radius stream matches the regular draw path
// across outlined, filled, and partially collapsed-inner asymmetric shapes.
TEST(
    SmoothShapes,
    ThickRoundRectCornerRadiiStreamingMatchesRegularPathAcrossInterestingCases) {
  for (const RoundRectCornersStreamParityCase& test_case :
       kRoundRectCornersStreamParityCases) {
    SCOPED_TRACE(test_case.basename);
    const SmoothShape shape = SmoothThickRoundRect(
        test_case.x0, test_case.y0, test_case.x1, test_case.y1, test_case.radii,
        test_case.thickness, test_case.outline, test_case.interior);

    const FakeOffscreen<Argb8888> actual = CoercedToViaStreaming(
        shape, Argb8888(), test_case.background, FillMode::kVisible,
        BlendingMode::kSourceOver, test_case.background);
    const FakeOffscreen<Argb8888> expected = CoercedTo<Argb8888>(
        shape, Argb8888(), test_case.background, FillMode::kVisible,
        BlendingMode::kSourceOver, test_case.background);

    EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
  }
}

// Verifies the unequal-radius stream still matches point-sampled rasterization
// when clipping begins and ends mid-row.
TEST(SmoothShapes,
     ThickRoundRectCornerRadiiStreamingMatchesRegularPathWhenClipped) {
  const SmoothShape shape = SmoothThickRoundRect(
      1.25f, 1.75f, 17.75f, 13.25f, RoundRectRadii{4.0f, 2.5f, 1.5f, 3.5f},
      1.5f, color::Black.withA(0x95), Color(0x40F3EFE7));
  const Box clip_box(3, 3, 16, 12);

  const FakeOffscreen<Argb8888> actual = CoercedToClipped(
      ForcedStreamable(&shape), clip_box, Argb8888(), color::Transparent,
      FillMode::kVisible, BlendingMode::kSourceOver, color::White);
  const FakeOffscreen<Argb8888> expected = CoercedToClipped(
      ForcedRasterizable(&shape), clip_box, Argb8888(), color::Transparent,
      FillMode::kVisible, BlendingMode::kSourceOver, color::White);

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

// Verifies outlines thicker than the centerline radius keep a stable shrunken
// inner round rect before the shape reaches the filled fold.
TEST(SmoothShapes,
     ThickRoundRectWithOutlineThickerThanRadiusRendersExpectedAlpha4Raster) {
  EXPECT_THAT(CoercedTo<Alpha4>(
                  SmoothThickRoundRect(2.5f, 2.5f, 12.5f, 12.5f, 2.0f, 5.0f,
                                       color::Black, color::Transparent),
                  Alpha4(color::Black)),
              MatchesContent(Alpha4(color::Black), Box(0, 0, 15, 15),
                             "0003777777773000"
                             "00AFFFFFFFFFFA00"
                             "0AFFFFFFFFFFFFA0"
                             "3FFFFFFFFFFFFFF3"
                             "7FFFFFFFFFFFFFF7"
                             "7FFFF777777FFFF7"
                             "7FFFF700007FFFF7"
                             "7FFFF700007FFFF7"
                             "7FFFF700007FFFF7"
                             "7FFFF700007FFFF7"
                             "7FFFF777777FFFF7"
                             "7FFFFFFFFFFFFFF7"
                             "3FFFFFFFFFFFFFF3"
                             "0AFFFFFFFFFFFFA0"
                             "00AFFFFFFFFFFA00"
                             "0003777777773000"));
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
     ThickRoundRectWithThinOutlineRendersExpectedTransparentArgb8888Raster) {
  for (const ThinOutlineAlphaContrastReproCase& repro_case :
       kThinOutlineAlphaContrastReproCases) {
    SCOPED_TRACE(repro_case.basename);
    const FakeOffscreen<Argb8888> actual =
        RenderThinOutlineRepro(kAlphaContrastReproThickness, repro_case.outline,
                               repro_case.interior, 1, color::Transparent);

    EXPECT_THAT(RasterOf(actual),
                MatchesContent(Argb8888(),
                               Box(0, 0, kThinOutlineReproWidth - 1,
                                   kThinOutlineReproHeight - 1),
                               repro_case.expected_raster));
  }
}

}  // namespace roo_display


#include "roo_display/core/rasterizable.h"

#include "roo_display/color/color.h"
#include "testing.h"

// Tests drawing and clipping rasterizables via their default drawTo method, and
// via createStream.

using namespace testing;

namespace roo_display {

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Box& clip_box,
          const Drawable& object, FillMode fill_mode = FillMode::kVisible,
          BlendingMode blending_mode = BlendingMode::kSourceOver,
          Color bgcolor = color::Transparent) {
  output.begin();
  Surface s(output, x, y, clip_box, false, bgcolor, fill_mode, blending_mode);
  s.drawObject(object);
  output.end();
}

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Drawable& object,
          FillMode fill_mode = FillMode::kVisible,
          BlendingMode blending_mode = BlendingMode::kSourceOver,
          Color bgcolor = color::Transparent) {
  Box clip_box(0, 0, output.effective_width() - 1,
               output.effective_height() - 1);
  Draw(output, x, y, clip_box, object, fill_mode, blending_mode, bgcolor);
}

// typedef bool (*setter_fn)(int16_t, int16_t);

typedef std::function<Color(int16_t, int16_t)> setter_fn;

setter_fn circle(int16_t x0, int16_t y0, int16_t r) {
  return [x0, y0, r](int16_t x, int16_t y) -> Color {
    if (x < x0) x = x0 + x0 - x;
    if (y < y0) y = y0 + y0 - y;
    return (x - x0) * (x - x0) + (y - y0) * (y - y0) < r * r - 1 ? color::White
                                                                 : color::Black;
  };
}

TEST(Rasterizable, SimpleFilledCircle) {
  auto input = MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3),
                                TransparencyMode::kNone);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(test_screen, 1, 2, input);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  ***    "
                                          " *****   "
                                          " *****   "
                                          " *****   "
                                          "  ***    "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, SimpleFilledCircleClipped) {
  auto input = MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3),
                                TransparencyMode::kNone);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(test_screen, 1, 2, Box(0, 0, 3, 4), input);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  **     "
                                          " ***     "
                                          " ***     "
                                          "         "
                                          "         "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, SimpleFilledCircleAsStreamable) {
  auto input = MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3),
                                TransparencyMode::kNone);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(test_screen, 1, 2, ForcedStreamable(&input));
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  ***    "
                                          " *****   "
                                          " *****   "
                                          " *****   "
                                          "  ***    "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, SimpleFilledCircleClippedAsStreamable) {
  auto input = MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3),
                                TransparencyMode::kNone);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(test_screen, 1, 2, Box(0, 0, 3, 4), ForcedStreamable(&input));
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  **     "
                                          " ***     "
                                          " ***     "
                                          "         "
                                          "         "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, StreamReportsRunLengthForSameRowUniformPrefix) {
  auto input = MakeRasterizable(
      Box(0, 0, 4, 1),
      [](int16_t x, int16_t y) -> Color {
        (void)x;
        return y == 0 ? color::White : color::Black;
      },
      TransparencyMode::kNone);

  auto stream = input.createStream();
  Color buf[3];
  uint32_t run_length = 0;
  stream->read(buf, 3, run_length);

  EXPECT_EQ(run_length, 3u);
  EXPECT_EQ(buf[0], color::White);
  EXPECT_EQ(buf[1], color::White);
  EXPECT_EQ(buf[2], color::White);
}

TEST(Rasterizable, StreamReportsZeroRunLengthForFallbackPaths) {
  auto varying = MakeRasterizable(
      Box(0, 0, 4, 1),
      [](int16_t x, int16_t y) -> Color {
        (void)y;
        return x == 0 ? color::White : color::Black;
      },
      TransparencyMode::kNone);

  {
    auto stream = varying.createStream();
    Color buf[3];
    uint32_t run_length = 0;
    stream->read(buf, 3, run_length);
    EXPECT_EQ(run_length, 0u);
  }

  auto per_row_constant = MakeRasterizable(
      Box(0, 0, 4, 1),
      [](int16_t x, int16_t y) -> Color {
        (void)x;
        return y == 0 ? color::White : color::Black;
      },
      TransparencyMode::kNone);

  {
    auto stream = per_row_constant.createStream();
    Color buf[6];
    uint32_t run_length = 0;
    stream->read(buf, 6, run_length);
    EXPECT_EQ(run_length, 0u);
  }
}

// Verifies small rectangles resolve the background exactly as tiled drawing
// and streaming do, including partially transparent backgrounds.
TEST(Rasterizable, BackgroundAcrossTileThreshold) {
  for (int width : {8, 9}) {
    Box bounds(0, 0, width - 1, 7);
    auto input = MakeRasterizable(bounds, [](int16_t x, int16_t y) {
      return x % 2 == 0 ? Color(0x80FF0000) : color::Transparent;
    });
    for (Color background :
         {color::Transparent, Color(0x800000FF), color::Blue}) {
      SCOPED_TRACE(width);
      SCOPED_TRACE(background);
      FakeOffscreen<Argb8888> raster(width, 8, color::Magenta);
      FakeOffscreen<Argb8888> streamed(width, 8, color::Magenta);
      Draw(raster, 0, 0, input, FillMode::kExtents, BlendingMode::kSource,
           background);
      Draw(streamed, 0, 0, ForcedStreamable(&input), FillMode::kExtents,
           BlendingMode::kSource, background);
      for (int i = 0; i < bounds.area(); ++i) {
        Color source =
            i % width % 2 == 0 ? Color(0x80FF0000) : color::Transparent;
        Color expected = AlphaBlend(background, source);
        EXPECT_EQ(raster.buffer()[i], expected);
        EXPECT_EQ(streamed.buffer()[i], expected);
      }
    }
  }
}

// Verifies every raster stream fast path clears previous run metadata when
// a uniform read is followed by a nonuniform read, and empty reads do nothing.
TEST(Rasterizable, StreamResetsRunMetadata) {
  for (int width : {1, 16, 40}) {
    auto input = MakeRasterizable(
        Box(0, 0, width - 1, 15), [width](int16_t x, int16_t y) {
          int index = y * width + x;
          return index < 4 ? color::Red
                           : (index % 2 == 0 ? color::Green : color::Blue);
        });
    auto stream = input.createStream();
    Color pixels[4];
    uint32_t run = 123;
    stream->read(nullptr, 0, run);
    EXPECT_EQ(run, 0u);
    stream->read(pixels, 4, run);
    ASSERT_EQ(run, 4u);
    stream->read(pixels, 4, run);
    EXPECT_EQ(run, 0u);
    EXPECT_NE(pixels[0], pixels[1]);
  }
}

namespace {

// Rejects oversized coordinate batches while preserving a coordinate oracle.
class BoundedPointRasterizable : public Rasterizable {
 public:
  Box extents() const override { return Box(0, 0, 319, 239); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    EXPECT_LE(count, 64u);
    for (uint32_t i = 0; i < count; ++i) {
      result[i] = Color(0xFF000000u | (y[i] * 320 + x[i]));
    }
  }
};

}  // namespace

// Verifies full-screen rectangle reads and large cross-row stream reads use
// bounded coordinate batches without losing row-major position.
TEST(Rasterizable, LargeReadsUseBoundedPointBatches) {
  BoundedPointRasterizable input;
  std::vector<Color> pixels(input.extents().area());
  EXPECT_FALSE(input.readColorRect(0, 0, 319, 239, pixels.data()));
  for (size_t i = 0; i < pixels.size(); ++i) {
    ASSERT_EQ(pixels[i], Color(0xFF000000u | i));
  }
  auto stream = input.createStream();
  stream->skip(3);
  stream->read(pixels.data(), 65535);
  for (int i = 0; i < 65535; ++i) {
    ASSERT_EQ(pixels[i], Color(0xFF000000u | (i + 3)));
  }
}

}  // namespace roo_display


#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/filter/transformation.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

TEST(Transformed, PositiveShift) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().translate(1, 2), &input);
  screen.Draw(transformed, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " *** "
                                     " *   "
                                     "     "));
}

TEST(Transformed, NegativeShift) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 3, 2),
                                "****"
                                "*** "
                                "****");
  TransformedDrawable transformed(Transformation().translate(-2, -1), &input);
  screen.Draw(transformed, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "*    "
                                     "**   "
                                     "     "
                                     "     "
                                     "     "));
}

TEST(Transformed, HorizontalFlip) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().flipX().translate(2, 0),
                                  &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " *** "
                                     "   * "
                                     "     "));
}

TEST(Transformed, VerticalFlip) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().flipY().translate(0, 1),
                                  &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " *   "
                                     " *** "
                                     "     "));
}

TEST(Transformed, HorizontalScale) {
  FakeScreen<Rgb565> screen(11, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().scale(3, 1), &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 11, 5,
                                     "           "
                                     "           "
                                     " ********* "
                                     " ***       "
                                     "           "));
}

TEST(Transformed, VerticalScale) {
  FakeScreen<Rgb565> screen(5, 9, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().scale(1, 3), &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 9,
                                     "     "
                                     "     "
                                     " *** "
                                     " *** "
                                     " *** "
                                     " *   "
                                     " *   "
                                     " *   "
                                     "     "));
}

TEST(Transformed, rotateRight) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().rotateRight(), &input);
  screen.Draw(transformed, 2, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " **  "
                                     "  *  "
                                     "  *  "));
}

TEST(Transformed, rotateLeft) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().rotateLeft(), &input);
  screen.Draw(transformed, 1, 3);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     " *   "
                                     " *   "
                                     " **  "
                                     "     "));
}

TEST(Transformed, rotateUpsideDown) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().rotateUpsideDown(), &input);
  screen.Draw(transformed, 3, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "   * "
                                     " *** "
                                     "     "
                                     "     "));
}

TEST(Transformed, SwapXY) {
  FakeScreen<Rgb565> screen(2, 3, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transformation().swapXY(), &input);
  screen.Draw(transformed, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 2, 3,
                                     "**"
                                     "* "
                                     "* "));
}

TEST(Transformed, Complex) {
  FakeScreen<Rgb565> screen(6, 11, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  Transformation t =
      Transformation().translate(2, 3).scale(-3, -2).rotateRight();
  TransformedDrawable transformed(t, &input);
  screen.Draw(transformed, -5, 15);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 6, 11,
                                     "      "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **** "
                                     " **** "
                                     " **** "
                                     "      "));
}

TEST(Transformed, ComplexTruncated) {
  FakeScreen<Rgb565> screen(2, 2, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), Box(0, 0, 2, 1),
                                "***"
                                "*  ");
  Transformation t =
      Transformation().translate(2, 3).scale(-3, -2).rotateRight();
  TransformedDrawable transformed(t, &input);
  screen.Draw(transformed, -7, 9);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 2, 2,
                                     "* "
                                     "**"));
}

namespace {
class TransformedCountingOutput : public FakeOffscreen<Argb8888> {
 public:
  using FakeOffscreen::FakeOffscreen;

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    rectangles += count;
    FakeOffscreen::fillRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t count) override {
    pixels += count;
    FakeOffscreen::fillPixels(mode, color, x, y, count);
  }

  uint32_t rectangles = 0;
  uint32_t pixels = 0;
};
}  // namespace

// Verifies a transformed full window stays one rectangle, and an unaligned
// span uses at most a leading row, a full-row block, and a trailing row.
TEST(Transformed, UniformFillUsesBoundedRectangles) {
  TransformedCountingOutput output(40, 40);
  TransformedDisplayOutput transformed(
      output, Transformation().scale(2, -1).swapXY().translate(20, 0));
  transformed.setAddress(1, 2, 8, 7, BlendingMode::kSource);
  transformed.fill(color::Blue, 48);
  EXPECT_EQ(output.rectangles, 1u);
  EXPECT_EQ(output.pixels, 0u);
  output.rectangles = 0;
  transformed.setAddress(1, 2, 8, 7, BlendingMode::kSource);
  transformed.fill(color::Red, 3);
  transformed.fill(color::Green, 34);
  EXPECT_EQ(output.rectangles, 4u);
  EXPECT_EQ(output.pixels, 0u);
}

// Verifies merged fills have exactly the pixel oracle's coverage and blending
// under swaps, flips, unequal scales, clipping, and interleaved dense writes.
TEST(Transformed, BatchedFillMatchesPixelOracle) {
  const Box source(2, 1, 8, 5);
  const Box clip(4, 6, 25, 27);
  for (bool swap : {false, true}) {
    for (int sx : {-2, -1, 1, 2}) {
      for (int sy : {-3, -1, 1, 3}) {
        if (!swap && sx == 1 && sy == 1) continue;
        Transformation transform = Transformation().scale(sx, sy);
        if (swap) transform = transform.swapXY();
        transform = transform.translate(18, 18).clip(clip);
        for (BlendingMode mode :
             {BlendingMode::kSource, BlendingMode::kSourceOver,
              BlendingMode::kSourceIn, BlendingMode::kSourceOut,
              BlendingMode::kSourceAtop, BlendingMode::kDestination,
              BlendingMode::kDestinationOver, BlendingMode::kDestinationIn,
              BlendingMode::kDestinationOut, BlendingMode::kDestinationAtop,
              BlendingMode::kClear, BlendingMode::kXor,
              BlendingMode::kSourceOverOpaque,
              BlendingMode::kDestinationOverOpaque}) {
          FakeOffscreen<Argb8888> actual(35, 35, Color(0x80503010));
          FakeOffscreen<Argb8888> expected(35, 35, Color(0x80503010));
          TransformedDisplayOutput output(actual, transform);
          output.setAddress(source.xMin(), source.yMin(), source.xMax(),
                            source.yMax(), mode);
          output.fill(color::Red, 0);
          int offset = 0;
          int step = 0;
          for (int count : {3, 5, 16, 1, 10}) {
            std::vector<Color> colors(count);
            bool fill = step % 2 == 0;
            for (int i = 0; i < count; ++i) {
              colors[i] =
                  fill ? Color(0x80602040) : Color(128, i * 23, 100, 40);
              int x = source.xMin() + (offset + i) % source.width();
              int y = source.yMin() + (offset + i) / source.width();
              Box target =
                  Box::Intersect(transform.transformBox(Box(x, y, x, y)), clip);
              if (!target.empty()) expected.fillRect(mode, target, colors[i]);
            }
            if (fill)
              output.fill(colors[0], count);
            else
              output.write(colors.data(), count);
            offset += count;
            ++step;
          }
          EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)))
              << swap << "/" << sx << "/" << sy << "/"
              << static_cast<int>(mode);
        }
      }
    }
  }
}

}  // namespace roo_display

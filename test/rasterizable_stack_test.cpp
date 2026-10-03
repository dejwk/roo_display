#include "roo_display/composition/rasterizable_stack.h"

#include <algorithm>
#include <vector>

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/composition/streamable_stack.h"
#include "roo_display/shape/basic.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

namespace {

class TransparentUniformProbeRasterizable : public Rasterizable {
 public:
  explicit TransparentUniformProbeRasterizable(Box extents)
      : extents_(extents) {}

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    for (uint32_t i = 0; i < count; ++i) {
      result[i] = color::Transparent;
    }
  }

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override {
    ++read_color_rect_calls_;
    *result = color::Transparent;
    return true;
  }

  bool readUniformColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                            int16_t yMax, Color* result) const override {
    ++read_uniform_color_rect_calls_;
    *result = color::Transparent;
    return true;
  }

  TransparencyMode getTransparencyMode() const override {
    return TransparencyMode::kFull;
  }

  int readColorRectCalls() const { return read_color_rect_calls_; }

  int readUniformColorRectCalls() const {
    return read_uniform_color_rect_calls_;
  }

 private:
  Box extents_;
  mutable int read_color_rect_calls_ = 0;
  mutable int read_uniform_color_rect_calls_ = 0;
};

// Counts every pixel-reading entry point, including conservative uniform
// checks.
class ReadCountingRasterizable : public Rasterizable {
 public:
  explicit ReadCountingRasterizable(const Rasterizable& source)
      : source_(source) {}

  Box extents() const override { return source_.extents(); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    ++read_calls_;
    source_.readColors(x, y, count, result);
  }

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override {
    ++read_calls_;
    return source_.readColorRect(xMin, yMin, xMax, yMax, result);
  }

  bool readUniformColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                            int16_t yMax, Color* result) const override {
    ++read_calls_;
    return source_.readUniformColorRect(xMin, yMin, xMax, yMax, result);
  }

  int readCalls() const { return read_calls_; }

 private:
  const Rasterizable& source_;
  mutable int read_calls_ = 0;
};

// Checks point reads, rectangular reads, and any claimed uniform result against
// the same pixel oracle. Rectangles may cover only part of the composition.
template <typename Expected>
void CheckCompositionReads(const RasterizableStack& stack, const Box& bounds,
                           Expected expected) {
  int count = bounds.area();
  std::vector<int16_t> x(count);
  std::vector<int16_t> y(count);
  std::vector<Color> pixels(count, Color(0xDEADBEEF));
  for (int i = 0; i < count; ++i) {
    x[i] = bounds.xMin() + i % bounds.width();
    y[i] = bounds.yMin() + i / bounds.width();
  }
  stack.readColors(x.data(), y.data(), count, pixels.data());
  for (int i = 0; i < count; ++i) {
    ASSERT_EQ(pixels[i], expected(x[i], y[i])) << "point " << i;
  }
  bool uniform =
      stack.readColorRect(bounds.xMin(), bounds.yMin(), bounds.xMax(),
                          bounds.yMax(), pixels.data());
  for (int i = 0; i < count; ++i) {
    ASSERT_EQ(pixels[uniform ? 0 : i], expected(x[i], y[i])) << "rect " << i;
  }
  Color uniform_color;
  if (stack.readUniformColorRect(bounds.xMin(), bounds.yMin(), bounds.xMax(),
                                 bounds.yMax(), &uniform_color)) {
    for (int i = 0; i < count; ++i) {
      ASSERT_EQ(uniform_color, expected(x[i], y[i])) << "uniform " << i;
    }
  }
}

// Checks successive stream reads that cross source bounds and row boundaries.
template <typename Expected>
void CheckCompositionStream(PixelStream& stream, const Box& bounds,
                            Expected expected) {
  Color pixels[67];
  for (int offset = 0; offset < bounds.area();) {
    int count = std::min(67, bounds.area() - offset);
    FillColor(pixels, count, Color(0xDEADBEEF));
    stream.read(pixels, count);
    for (int i = 0; i < count; ++i) {
      int16_t x = bounds.xMin() + (offset + i) % bounds.width();
      int16_t y = bounds.yMin() + (offset + i) / bounds.width();
      ASSERT_EQ(pixels[i], expected(x, y)) << "stream at " << x << ", " << y;
    }
    offset += count;
  }
}

}  // namespace

TEST(RasterizableStack, Empty) {
  RasterizableStack stack(Box(3, 4, 5, 7));
  EXPECT_EQ(stack.naturalExtents(), Box(0, 0, -1, -1));
  FakeOffscreen<Rgb565> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, SingleUnclipped) {
  auto input = MakeTestRasterizable(Grayscale4(), Box(0, 0, 3, 3),
                                    "1234"
                                    "2345"
                                    "3456"
                                    "4567");
  RasterizableStack stack(Box(3, 4, 9, 10));
  stack.addInput(&input, 5, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "     1234 "
                                          "     2345 "
                                          "     3456 "
                                          "     4567 "
                                          "          "));
}

TEST(RasterizableStack, SingleNegativeOffset) {
  auto input = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 1),
                                    "123"
                                    "456");
  RasterizableStack stack(Box(0, 0, 3, 4));
  stack.addInput(&input, -1, 2);
  EXPECT_EQ(stack.naturalExtents(), Box(-1, 2, 1, 3));
  FakeOffscreen<Argb4444> test_screen(4, 5, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 4, 5,
                                          "    "
                                          "    "
                                          "23  "
                                          "56  "
                                          "    "));
}

TEST(RasterizableStack, UnsignedCallerVariables) {
  auto input = MakeTestRasterizable(Grayscale4(), Box(0, 0, 3, 3),
                                    "1234"
                                    "2345"
                                    "3456"
                                    "4567");
  RasterizableStack stack(Box(3, 4, 9, 10));
  uint16_t dx = 5;
  uint16_t dy = 6;
  stack.addInput(&input, dx, dy);
  EXPECT_EQ(stack.naturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "     1234 "
                                          "     2345 "
                                          "     3456 "
                                          "     4567 "
                                          "          "));
}

TEST(RasterizableStack, SingleClipped) {
  auto input = MakeTestRasterizable(Grayscale4(), Box(0, 0, 3, 3),
                                    "1234"
                                    "2345"
                                    "3456"
                                    "4567");
  RasterizableStack stack(Box(3, 4, 9, 10));
  stack.addInput(&input, 5, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setClipBox(6, 7, 7, 8);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "      34  "
                                          "      45  "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, SingleSelfClipped) {
  auto input = MakeTestRasterizable(Grayscale4(), Box(0, 0, 3, 3),
                                    "1234"
                                    "2345"
                                    "3456"
                                    "4567");
  RasterizableStack stack(Box(3, 4, 9, 10));
  stack.addInput(&input, Box(1, 1, 2, 2), 5, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(6, 7, 7, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "      34  "
                                          "      45  "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, TwoDisjoint) {
  auto input1 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "123"
                                     "234"
                                     "345");
  auto input2 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "666"
                                     "777"
                                     "888");
  RasterizableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 6, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 8, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  234     "
                                          "  345     "
                                          "          "
                                          "      666 "
                                          "      777 "
                                          "      888 "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, TwoDisjointInherentlyClipped) {
  auto input1 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "123"
                                     "234"
                                     "345");
  auto input2 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "666"
                                     "777"
                                     "888");
  RasterizableStack stack(Box(3, 3, 7, 7));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 6, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 8, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "   34     "
                                          "   45     "
                                          "          "
                                          "      66  "
                                          "      77  "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, TwoHorizontalOverlap) {
  auto input1 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "123"
                                     "234"
                                     "345");
  auto input2 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "666"
                                     "777"
                                     "888");
  RasterizableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 6, 3);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 8, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  234 666 "
                                          "  345 777 "
                                          "      888 "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, TwoVerticalOverlap) {
  auto input1 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "123"
                                     "234"
                                     "345");
  auto input2 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "666"
                                     "777"
                                     "888");
  RasterizableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 3, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 5, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  234     "
                                          "  345     "
                                          "          "
                                          "   666    "
                                          "   777    "
                                          "   888    "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, TwoOverlap) {
  auto input1 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "123"
                                     "234"
                                     "345");
  auto input2 = MakeTestRasterizable(Grayscale4(), Box(0, 0, 2, 2),
                                     "666"
                                     "777"
                                     "888");
  RasterizableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 3, 3);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 5, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  2666    "
                                          "  3777    "
                                          "   888    "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, TwoOverlapAlphaBlend) {
  auto input1 = MakeTestRasterizable(Alpha4(color::White), Box(0, 0, 2, 2),
                                     "567"
                                     "678"
                                     "789");
  auto input2 = MakeTestRasterizable(Alpha4(color::White), Box(0, 0, 2, 2),
                                     "666"
                                     "777"
                                     "888");
  RasterizableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 3, 3);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 5, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  567     "
                                          "  6AB6    "
                                          "  7BC7    "
                                          "   888    "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(RasterizableStack, ReadColorRectSkipsTransparentPartialLayer) {
  Fill red_fill(color::Red);
  TransparentUniformProbeRasterizable transparent_overlay(Box(0, 0, 4, 4));

  RasterizableStack stack(Box(0, 0, 9, 9));
  stack.addInput(&red_fill, Box(0, 0, 9, 9));
  stack.addInput(&transparent_overlay, Box(0, 0, 4, 4));

  Color result;
  EXPECT_TRUE(stack.readColorRect(0, 0, 9, 9, &result));
  EXPECT_EQ(result, color::Red);
  EXPECT_EQ(transparent_overlay.readUniformColorRectCalls(), 1);
  EXPECT_EQ(transparent_overlay.readColorRectCalls(), 0);
}

// Verifies explicit clear samples paint Background while absent samples reset
// the accumulated result to Transparent throughout the composition.
TEST(RasterizableStack, ReadColorRectDoesNotSkipTransparentPartialClearLayer) {
  Fill red_fill(color::Red);
  TransparentUniformProbeRasterizable transparent_overlay(Box(0, 0, 1, 1));

  RasterizableStack stack(Box(0, 0, 2, 2));
  stack.addInput(&red_fill, Box(0, 0, 2, 2));
  stack.addInput(&transparent_overlay, Box(0, 0, 1, 1))
      .withMode(BlendingMode::kClear);

  Color result[9];
  EXPECT_FALSE(stack.readColorRect(0, 0, 2, 2, result));
  EXPECT_EQ(result[0], color::Background);
  EXPECT_EQ(result[1], color::Background);
  EXPECT_EQ(result[2], color::Transparent);
  EXPECT_EQ(result[3], color::Background);
  EXPECT_EQ(result[4], color::Background);
  EXPECT_EQ(result[5], color::Transparent);
  EXPECT_EQ(result[6], color::Transparent);
  EXPECT_EQ(result[7], color::Transparent);
  EXPECT_EQ(result[8], color::Transparent);
}

// Verifies arbitrary input counts retain all blend operations across the
// compiler threshold, including absent sources which clear previous layers.
TEST(RasterizableStack, ExcessInputsUseRasterStreaming) {
  for (int width : {128, 129, 320}) {
    Box bounds(0, 0, width - 1, 0);
    FilledRect red(bounds, color::Red);
    FilledRect blue(Box(7, 0, width - 4, 0), color::Blue);
    for (int count : {17, 32}) {
      RasterizableStack stack(bounds);
      for (int i = 0; i < count - 2; ++i) stack.addInput(&red);
      stack.addInput(&red, 0, 2).withMode(BlendingMode::kDestinationIn);
      stack.addInput(&blue);
      auto expected = [&blue](int16_t x, int16_t y) {
        return blue.extents().contains(x, y) ? color::Blue : color::Transparent;
      };
      CheckCompositionStream(*stack.createStream(), bounds, expected);
      Box clip(3, 0, width - 2, 0);
      CheckCompositionStream(*stack.createStream(clip), clip, expected);
    }
  }
}

// Verifies all ordinary first-input modes agree across the 128/129-pixel
// threshold.
TEST(RasterizableStack, FirstInputBlendingAcrossCompilerThreshold) {
  for (int width : {128, 129}) {
    Box bounds(0, 0, width - 1, 0);
    for (int mode_index = static_cast<int>(BlendingMode::kSource);
         mode_index <= static_cast<int>(BlendingMode::kXor); ++mode_index) {
      BlendingMode mode = static_cast<BlendingMode>(mode_index);
      for (Color source : {color::Transparent, Color(0x80654321), color::Red,
                           color::Background}) {
        SCOPED_TRACE(width);
        SCOPED_TRACE(mode_index);
        FilledRect input(bounds, source);
        RasterizableStack stack(bounds);
        stack.addInput(&input).withMode(mode);
        Color want = ApplyBlending(mode, color::Transparent, source);
        for (bool clipped : {false, true}) {
          std::unique_ptr<PixelStream> stream =
              clipped ? stack.createStream(bounds) : stack.createStream();
          Color pixels[129];
          FillColor(pixels, width, Color(0xDEADBEEF));
          stream->read(pixels, width);
          for (int i = 0; i < width; ++i) ASSERT_EQ(pixels[i], want);
        }
      }
    }
  }
}

// Verifies raster reads and small streams keep supporting more than sixteen
// inputs.
TEST(RasterizableStack, RasterInputCapacityIsUnchanged) {
  Box bounds(0, 0, 127, 0);
  FilledRect red(bounds, color::Red);
  FilledRect blue(bounds, color::Blue);
  RasterizableStack stack(bounds);
  for (int i = 0; i < 16; ++i) stack.addInput(&red);
  stack.addInput(&blue);
  int16_t x = 0;
  int16_t y = 0;
  Color result;
  stack.readColors(&x, &y, 1, &result);
  EXPECT_EQ(result, color::Blue);
  for (bool clipped : {false, true}) {
    std::unique_ptr<PixelStream> stream =
        clipped ? stack.createStream(bounds) : stack.createStream();
    stream->read(&result, 1);
    EXPECT_EQ(result, color::Blue);
  }
}

// Verifies every ordinary mode uses the same source-clipped, composition-wide
// semantics in raster reads and streams, above and below the compiler
// threshold.
TEST(RasterizableStack, PartialInputBoundsAcrossReadPaths) {
  const struct {
    BlendingMode mode;
    bool clears_outside;
  } cases[] = {
      {BlendingMode::kSource, true},
      {BlendingMode::kSourceOver, false},
      {BlendingMode::kSourceIn, true},
      {BlendingMode::kSourceAtop, false},
      {BlendingMode::kDestination, false},
      {BlendingMode::kDestinationOver, false},
      {BlendingMode::kDestinationIn, true},
      {BlendingMode::kDestinationAtop, true},
      {BlendingMode::kClear, true},
      {BlendingMode::kSourceOut, true},
      {BlendingMode::kDestinationOut, false},
      {BlendingMode::kXor, false},
  };
  for (int height : {8, 9}) {
    Box bounds(0, 0, 15, height - 1);
    for (const auto& test : cases) {
      for (Color base : {color::Red, Color(0x80654321), color::Transparent,
                         color::Background}) {
        for (Color sample :
             {color::White, Color(0x80123456), color::Transparent,
              color::Background, Color(0x00123456)}) {
          for (Box source_clip : {Box(12, 22, 21, 25), Box(40, 40, 41, 41)}) {
            SCOPED_TRACE(height);
            SCOPED_TRACE(static_cast<int>(test.mode));
            SCOPED_TRACE(base);
            SCOPED_TRACE(sample);
            SCOPED_TRACE(source_clip);
            FilledRect lower(bounds, base);
            FilledRect upper(Box(10, 20, 29, 29), sample);
            RasterizableStack raster(bounds);
            raster.addInput(&lower).withMode(BlendingMode::kSource);
            raster.addInput(&upper, source_clip, -8, -20).withMode(test.mode);
            StreamableStack compiled(bounds);
            compiled.addInput(&lower).withMode(BlendingMode::kSource);
            compiled.addInput(&upper, source_clip, -8, -20).withMode(test.mode);
            Box covered =
                Box::Intersect(upper.extents(), source_clip).translate(-8, -20);
            auto expected = [&](int16_t x, int16_t y) {
              return covered.contains(x, y)
                         ? ApplyBlending(test.mode, base, sample)
                     : test.clears_outside ? color::Transparent
                                           : base;
            };
            CheckCompositionReads(raster, bounds, expected);
            CheckCompositionStream(*raster.createStream(), bounds, expected);
            CheckCompositionStream(*compiled.createStream(), bounds, expected);
            // Includes a 128-pixel clip of the larger stack, a wholly missing
            // source, full source coverage, and partial coverage of the source.
            for (Box clip : {Box(0, 0, 15, 7), Box(0, 0, 3, 1), Box(5, 3, 8, 4),
                             Box(2, 1, 8, 4)}) {
              CheckCompositionReads(raster, clip, expected);
              CheckCompositionStream(*raster.createStream(clip), clip,
                                     expected);
              CheckCompositionStream(*compiled.createStream(clip), clip,
                                     expected);
            }
          }
        }
      }
    }
  }
}

// Verifies masks also clear already materialized, nonuniform results, honor
// source clipping and translation, and leave later layers free to draw.
TEST(RasterizableStack, MaskNonuniformLayersAndDrawAboveThem) {
  Box bounds(0, 0, 15, 8);
  auto base_color = [](int16_t x, int16_t y) {
    return (x + y) % 2 == 0 ? color::Red : color::Green;
  };
  auto base = MakeRasterizable(bounds, base_color);
  Box source_bounds(10, 20, 17, 25);
  Box source_clip(12, 21, 15, 24);
  auto mask_color = [&](int16_t x, int16_t y) {
    EXPECT_TRUE(source_clip.contains(x, y));
    return (x + y) % 2 == 0 ? color::White : Color(0x80123456);
  };
  auto mask = MakeRasterizable(source_bounds, mask_color);
  FilledRect overlay(Box(0, 6, 2, 8), color::Blue);
  RasterizableStack stack(bounds);
  stack.addInput(&base);
  stack.addInput(&mask, source_clip, -9, -20)
      .withMode(BlendingMode::kDestinationIn);
  stack.addInput(&overlay);
  Box covered = source_clip.translate(-9, -20);
  auto expected = [&](int16_t x, int16_t y) {
    if (overlay.extents().contains(x, y)) return color::Blue;
    if (!covered.contains(x, y)) return color::Transparent;
    return ApplyBlending(BlendingMode::kDestinationIn, base_color(x, y),
                         mask_color(x + 9, y + 20));
  };
  CheckCompositionReads(stack, bounds, expected);
  CheckCompositionStream(*stack.createStream(), bounds, expected);
  Box clip(1, 0, 10, 7);
  CheckCompositionReads(stack, clip, expected);
  CheckCompositionStream(*stack.createStream(clip), clip, expected);

  // A wholly disjoint source can reset a materialized result to uniform, after
  // which the same overlay must still draw correctly.
  FilledRect missing(Box(50, 50, 51, 51), color::White);
  stack.addInput(&missing).withMode(BlendingMode::kSource);
  auto transparent = [](int16_t, int16_t) { return color::Transparent; };
  CheckCompositionReads(stack, bounds, transparent);
  CheckCompositionStream(*stack.createStream(), bounds, transparent);
  stack.addInput(&overlay);
  auto only_overlay = [&](int16_t x, int16_t y) {
    return overlay.extents().contains(x, y) ? color::Blue : color::Transparent;
  };
  CheckCompositionReads(stack, bounds, only_overlay);
  CheckCompositionStream(*stack.createStream(), bounds, only_overlay);
}

// Verifies the uniform fast path distinguishes explicit transparent samples
// from absent samples, including Background's exact placeholder value.
TEST(RasterizableStack, UniformReadsRespectAbsentSourcesAndBackground) {
  FilledRect base(Box(0, 0, 3, 3), color::Background);
  TransparentUniformProbeRasterizable overlay(Box(1, 1, 2, 2));
  RasterizableStack stack(base.extents());
  stack.addInput(&base).withMode(BlendingMode::kSource);
  stack.addInput(&overlay).withMode(BlendingMode::kDestinationOver);
  Color result;
  EXPECT_FALSE(stack.readUniformColorRect(0, 0, 3, 3, &result));
  EXPECT_TRUE(stack.readUniformColorRect(1, 1, 2, 2, &result));
  EXPECT_EQ(result, color::Transparent);
  EXPECT_TRUE(stack.readUniformColorRect(0, 0, 3, 0, &result));
  EXPECT_EQ(result, color::Background);

  stack.clearInputs();
  stack.addInput(&base).withMode(BlendingMode::kSource);
  stack.addInput(&overlay).withMode(BlendingMode::kSource);
  EXPECT_TRUE(stack.readUniformColorRect(0, 0, 3, 3, &result));
  EXPECT_EQ(result, color::Transparent);
  EXPECT_TRUE(stack.readColorRect(0, 0, 3, 3, &result));
  EXPECT_EQ(result, color::Transparent);
  EXPECT_EQ(overlay.readColorRectCalls(), 0);
}

// Verifies both rectangle methods skip all source reads when a final absent
// source clears the query, including translated, clipped, and empty sources.
TEST(RasterizableStack, RectangleClearSkipsAllSourceReads) {
  Box bounds(0, 0, 7, 7);
  auto nonuniform = MakeRasterizable(bounds, [](int16_t x, int16_t y) {
    return (x + y) % 2 == 0 ? color::Red : color::Green;
  });
  FilledRect white(bounds, color::White);
  for (BlendingMode mode :
       {BlendingMode::kSource, BlendingMode::kSourceIn,
        BlendingMode::kSourceOut, BlendingMode::kDestinationIn,
        BlendingMode::kDestinationAtop, BlendingMode::kClear}) {
    for (Box source_clip : {bounds, Box(4, 4, 7, 7), Box(2, 2, 1, 1)}) {
      SCOPED_TRACE(static_cast<int>(mode));
      SCOPED_TRACE(source_clip);
      ReadCountingRasterizable lower(nonuniform);
      ReadCountingRasterizable clearing(white);
      RasterizableStack stack(bounds);
      stack.addInput(&lower);
      int16_t offset = source_clip == bounds ? 4 : 0;
      stack.addInput(&clearing, source_clip, offset, offset).withMode(mode);

      Color uniform;
      EXPECT_TRUE(stack.readUniformColorRect(0, 0, 3, 3, &uniform));
      EXPECT_EQ(uniform, color::Transparent);
      Color pixels[16];
      FillColor(pixels, 16, Color(0xDEADBEEF));
      EXPECT_TRUE(stack.readColorRect(0, 0, 3, 3, pixels));
      EXPECT_EQ(pixels[0], color::Transparent);
      // The fully cleared result stays uniform without materializing pixels.
      for (int i = 1; i < 16; ++i) EXPECT_EQ(pixels[i], Color(0xDEADBEEF));
      EXPECT_EQ(lower.readCalls(), 0);
      EXPECT_EQ(clearing.readCalls(), 0);
    }
  }
}

// Verifies the last complete clear skips every earlier source, while later
// layers still blend and can make the result uniform or nonuniform.
TEST(RasterizableStack, RectangleReadsStartAfterLastCompleteClear) {
  Box bounds(0, 0, 7, 7);
  auto nonuniform = MakeRasterizable(bounds, [](int16_t x, int16_t y) {
    return (x + y) % 2 == 0 ? color::Red : color::Green;
  });
  FilledRect missing(Box(10, 10, 11, 11), color::White);
  FilledRect blue(bounds, color::Blue);
  ReadCountingRasterizable lower(nonuniform);
  ReadCountingRasterizable middle(nonuniform);
  ReadCountingRasterizable clearing(missing);
  ReadCountingRasterizable upper(blue);
  RasterizableStack stack(bounds);
  stack.addInput(&lower);
  stack.addInput(&clearing).withMode(BlendingMode::kSource);
  stack.addInput(&middle);
  stack.addInput(&clearing).withMode(BlendingMode::kDestinationIn);
  stack.addInput(&upper);
  stack.addInput(&clearing);  // An absent SourceOver input leaves blue intact.

  Color uniform;
  EXPECT_TRUE(stack.readUniformColorRect(0, 0, 7, 7, &uniform));
  EXPECT_EQ(uniform, color::Blue);
  Color pixels[64];
  EXPECT_TRUE(stack.readColorRect(0, 0, 7, 7, pixels));
  EXPECT_EQ(pixels[0], color::Blue);
  EXPECT_EQ(upper.readCalls(), 2);

  // A later mask that covers part of the query must retain the blue beneath it.
  FilledRect mask(Box(2, 2, 5, 5), color::White);
  stack.addInput(&mask).withMode(BlendingMode::kDestinationIn);
  EXPECT_FALSE(stack.readUniformColorRect(0, 0, 7, 7, &uniform));
  EXPECT_FALSE(stack.readColorRect(0, 0, 7, 7, pixels));
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(pixels[i], mask.extents().contains(i % 8, i / 8)
                             ? color::Blue
                             : color::Transparent);
  }
  EXPECT_EQ(lower.readCalls(), 0);
  EXPECT_EQ(middle.readCalls(), 0);
  EXPECT_EQ(clearing.readCalls(), 0);
}

// Verifies drawing clips evaluation without shrinking a mask's operation bounds
// and preserves visible/extents handling of the resulting transparent region.
TEST(RasterizableStack, DrawPartialMaskWithCompositionBounds) {
  Box bounds(0, 0, 15, 8);
  FilledRect base(bounds, color::Red);
  FilledRect mask(Box(4, 2, 11, 5), color::White);
  RasterizableStack stack(bounds);
  stack.addInput(&base);
  stack.addInput(&mask).withMode(BlendingMode::kDestinationIn);
  for (FillMode fill : {FillMode::kVisible, FillMode::kExtents}) {
    for (Box clip : {bounds, Box(2, 1, 13, 7), Box(0, 0, 3, 1)}) {
      FakeOffscreen<Argb8888> screen(18, 11, color::Magenta);
      Display display(screen);
      {
        DrawingContext dc(display);
        dc.setClipBox(clip);
        dc.setFillMode(fill);
        dc.setBackgroundColor(color::Transparent);
        dc.setBlendingMode(BlendingMode::kSource);
        dc.draw(stack);
      }
      for (int16_t y = 0; y < screen.raw_height(); ++y) {
        for (int16_t x = 0; x < screen.raw_width(); ++x) {
          Color expected = color::Magenta;
          if (clip.contains(x, y)) {
            if (mask.extents().contains(x, y)) {
              expected = color::Red;
            } else if (fill == FillMode::kExtents) {
              expected = color::Transparent;
            }
          }
          EXPECT_EQ(screen.buffer()[y * screen.raw_width() + x], expected)
              << "at " << x << ", " << y;
        }
      }
    }
  }
}

// Verifies small clipped streams intersect the requested clip with composition
// bounds, as compiled streams do, even when sources extend beyond those bounds.
TEST(RasterizableStack, SmallStreamClipsToCompositionBounds) {
  Box bounds(7, 9, 14, 16);
  auto expected = [](int16_t x, int16_t y) { return Color(x, y, 0); };
  auto source = MakeRasterizable(Box(0, 0, 20, 20), expected);
  RasterizableStack stack(bounds);
  stack.addInput(&source);
  CheckCompositionStream(*stack.createStream(Box(-2, -2, 15, 17)), bounds,
                         expected);
}

namespace {

// Checks the stack never forwards an unbounded rectangle to its children.
class BoundedRectangleRasterizable : public Rasterizable {
 public:
  explicit BoundedRectangleRasterizable(bool uniform) : uniform_(uniform) {}

  Box extents() const override { return Box(-5, -3, 314, 236); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    for (uint32_t i = 0; i < count; ++i) {
      result[i] = uniform_ ? color::Red
                           : Color(0xFF000000u | ((y[i] + 3) * 320 + x[i] + 5));
    }
  }

  bool readColorRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     Color* result) const override {
    EXPECT_LE(Box(x0, y0, x1, y1).area(), 64);
    return Rasterizable::readColorRect(x0, y0, x1, y1, result);
  }

 private:
  bool uniform_;
};

}  // namespace

// Verifies large nested reads keep child requests bounded, copy edge tiles
// correctly, and retain an implicit uniform result without writing its tail.
TEST(RasterizableStack, LargeNestedRectangleUsesBoundedTiles) {
  Box bounds(0, 0, 319, 239);
  for (bool uniform : {false, true}) {
    BoundedRectangleRasterizable source(uniform);
    RasterizableStack inner(bounds);
    inner.addInput(&source, 5, 3);
    RasterizableStack outer(bounds);
    outer.addInput(&inner);
    std::vector<Color> pixels(bounds.area(), Color(0xDEADBEEF));
    EXPECT_EQ(outer.readColorRect(1, 2, 318, 238, pixels.data()), uniform);
    if (uniform) {
      EXPECT_EQ(pixels[0], color::Red);
      EXPECT_EQ(pixels[1], Color(0xDEADBEEF));
    } else {
      for (int i = 0; i < 318 * 237; ++i) {
        ASSERT_EQ(pixels[i],
                  Color(0xFF000000u | ((i / 318 + 2) * 320 + i % 318 + 1)));
      }
    }
  }
}

// Verifies rebuilding and replacing inputs preserves stack/anchor bounds,
// refreshes clips and offsets, and resets an old mode without changing order.
TEST(RasterizableStack, ReuseAndReplaceInputs) {
  Box bounds(0, 0, 19, 9);
  FilledRect red(bounds, color::Red);
  FilledRect blue(Box(10, 10, 29, 19), color::Blue);
  RasterizableStack stack(bounds);
  stack.setAnchorExtents(Box(2, 2, 7, 7));
  stack.reserveInputs(4);
  stack.addInput(&red);
  stack.addInput(&blue).withMode(BlendingMode::kDestinationIn);
  EXPECT_EQ(stack.inputCount(), 2u);
  const auto& replaced =
      stack.setInput(1, &blue, Box(12, 12, 16, 16), -10, -10);
  EXPECT_EQ(replaced.extents(), Box(2, 2, 6, 6));
  EXPECT_EQ(replaced.blending_mode(), BlendingMode::kSourceOver);
  auto expected = [](int16_t x, int16_t y) {
    return Box(2, 2, 6, 6).contains(x, y) ? color::Blue : color::Red;
  };
  CheckCompositionStream(*stack.createStream(), bounds, expected);
  stack.clearInputs();
  EXPECT_EQ(stack.inputCount(), 0u);
  EXPECT_EQ(stack.extents(), bounds);
  EXPECT_EQ(stack.anchorExtents(), Box(2, 2, 7, 7));
  stack.addInput(&blue);
  stack.setInput(0, &red);
  CheckCompositionStream(*stack.createStream(), bounds,
                         [](int16_t, int16_t) { return color::Red; });
}

// Verifies replacing a missing layer is rejected before writing storage.
TEST(RasterizableStackDeathTest, InvalidReplacementIndex) {
  FilledRect red(Box(0, 0, 1, 1), color::Red);
  RasterizableStack stack(red.extents());
  EXPECT_DEATH(stack.setInput(0, &red), "index");
}

}  // namespace roo_display

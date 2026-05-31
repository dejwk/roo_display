
#include "roo_display/composition/rasterizable_stack.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
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
  EXPECT_EQ(result[2], color::Red);
  EXPECT_EQ(result[3], color::Background);
  EXPECT_EQ(result[4], color::Background);
  EXPECT_EQ(result[5], color::Red);
  EXPECT_EQ(result[6], color::Red);
  EXPECT_EQ(result[7], color::Red);
  EXPECT_EQ(result[8], color::Red);
}

}  // namespace roo_display

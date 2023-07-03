
#include "roo_display/composition/rasterizable_stack.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

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

}  // namespace roo_display

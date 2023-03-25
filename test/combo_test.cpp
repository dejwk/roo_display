
#include "roo_display/core/combo.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "testing.h"
// #include "offscreen.h"

using namespace testing;

namespace roo_display {

TEST(Combo, Empty) {
  Combo combo(Box(3, 4, 5, 7));
  EXPECT_EQ(combo.NaturalExtents(), Box(0, 0, -1, -1));
  FakeOffscreen<Rgb565> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, SingleUnclipped) {
  auto input = MakeTestStreamable(Grayscale4(), 4, 4,
                                  "1234"
                                  "2345"
                                  "3456"
                                  "4567");
  Combo combo(Box(3, 4, 9, 10));
  combo.AddInput(&input, 5, 6);
  EXPECT_EQ(combo.NaturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, SingleClipped) {
  auto input = MakeTestStreamable(Grayscale4(), 4, 4,
                                  "1234"
                                  "2345"
                                  "3456"
                                  "4567");
  Combo combo(Box(3, 4, 9, 10));
  combo.AddInput(&input, 5, 6);
  EXPECT_EQ(combo.NaturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setClipBox(6, 7, 7, 8);
    dc.draw(combo);
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

TEST(Combo, TwoDisjoint) {
  auto input1 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "123"
                                  "234"
                                  "345");
  auto input2 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "666"
                                  "777"
                                  "888");
  Combo combo(Box(1, 1, 9, 10));
  combo.AddInput(&input1, 2, 2);
  combo.AddInput(&input2, 6, 6);
  EXPECT_EQ(combo.NaturalExtents(), Box(2, 2, 8, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, TwoDisjointInherentlyClipped) {
  auto input1 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "123"
                                  "234"
                                  "345");
  auto input2 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "666"
                                  "777"
                                  "888");
  Combo combo(Box(3, 3, 7, 7));
  combo.AddInput(&input1, 2, 2);
  combo.AddInput(&input2, 6, 6);
  EXPECT_EQ(combo.NaturalExtents(), Box(2, 2, 8, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, TwoHorizontalOverlap) {
  auto input1 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "123"
                                  "234"
                                  "345");
  auto input2 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "666"
                                  "777"
                                  "888");
  Combo combo(Box(1, 1, 9, 10));
  combo.AddInput(&input1, 2, 2);
  combo.AddInput(&input2, 6, 3);
  EXPECT_EQ(combo.NaturalExtents(), Box(2, 2, 8, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, TwoVerticalOverlap) {
  auto input1 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "123"
                                  "234"
                                  "345");
  auto input2 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "666"
                                  "777"
                                  "888");
  Combo combo(Box(1, 1, 9, 10));
  combo.AddInput(&input1, 2, 2);
  combo.AddInput(&input2, 3, 6);
  EXPECT_EQ(combo.NaturalExtents(), Box(2, 2, 5, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, TwoOverlap) {
  auto input1 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "123"
                                  "234"
                                  "345");
  auto input2 = MakeTestStreamable(Grayscale4(), 3, 3,
                                  "666"
                                  "777"
                                  "888");
  Combo combo(Box(1, 1, 9, 10));
  combo.AddInput(&input1, 2, 2);
  combo.AddInput(&input2, 3, 3);
  EXPECT_EQ(combo.NaturalExtents(), Box(2, 2, 5, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
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

TEST(Combo, TwoOverlapAlphaBlend) {
  auto input1 = MakeTestStreamable(Alpha4(color::White), 3, 3,
                                  "123"
                                  "234"
                                  "345");
  auto input2 = MakeTestStreamable(Alpha4(color::White), 3, 3,
                                  "666"
                                  "777"
                                  "888");
  Combo combo(Box(1, 1, 9, 10));
  combo.AddInput(&input1, 2, 2);
  combo.AddInput(&input2, 3, 3);
  EXPECT_EQ(combo.NaturalExtents(), Box(2, 2, 5, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(combo);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  2886    "
                                          "  39A7    "
                                          "   888    "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

}  // namespace roo_display

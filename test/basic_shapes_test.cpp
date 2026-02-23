
#include "roo_display/color/color.h"
#include "roo_display/shape/basic.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(BasicShapes, FillRectOutOfBounds) {
  FakeOffscreen<Rgb565> test_screen(2, 3);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Intentionally write out-of-bounds to test that it is caught by
    // the shape.
    dc.draw(FilledRect(-5, -2, 8, 7, color::Black));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 2, 3,
                                          "  "
                                          "  "
                                          "  "));
}

TEST(BasicShapes, FillRectOpaque) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Draw the basic shape.
    dc.draw(FilledRect(1, 2, 2, 4, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          " **  "
                                          " **  "
                                          " **  "
                                          "     "));
}

TEST(BasicShapes, FillRectStreamable) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Draw the basic shape.
    auto rect = FilledRect(1, 2, 2, 4, color::White);
    dc.draw(ForcedStreamable(&rect));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          " **  "
                                          " **  "
                                          " **  "
                                          "     "));
}

TEST(BasicShapes, FillRectRasterizable) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Draw the basic shape.
    auto rect = FilledRect(1, 2, 2, 4, color::White);
    dc.draw(ForcedRasterizable(&rect));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          " **  "
                                          " **  "
                                          " **  "
                                          "     "));
}

TEST(BasicShapes, FillRectTransparent) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Lime);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Draw the basic shape #1
    dc.draw(FilledRect(1, 2, 2, 3, color::Blue));
    // And overlay a semi-transparent shape #2
    Color c = color::Red;
    c.set_a(0x40);
    dc.setBlendingMode(kBlendingSourceOver);
    dc.draw(FilledRect(2, 3, 3, 4, c));
  }
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "_*_ _*_ _*_ _*_ _*_"
                                          "_*_ _*_ _*_ _*_ _*_"
                                          "_*_ __* __* _*_ _*_"
                                          "_*_ __* D_k Dk_ _*_"
                                          "_*_ _*_ Dk_ Dk_ _*_"
                                          "_*_ _*_ _*_ _*_ _*_"));
}

TEST(BasicShapes, FillZeroRadiusCircle) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Draw the basic shape.
    dc.draw(FilledCircle::ByRadius(2, 3, 0, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          "     "
                                          "  *  "
                                          "     "
                                          "     "));
}

TEST(BasicShapes, DrawZeroRadiusCircle) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Draw the basic shape.
    dc.draw(Circle::ByRadius(2, 3, 0, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          "     "
                                          "  *  "
                                          "     "
                                          "     "));
}

TEST(BasicShapes, DrawZeroRadiusCircle2) {
  EXPECT_THAT(CoercedTo<Rgb565>(Circle::ByRadius(2, 3, 0, color::White)),
              MatchesContent(WhiteOnBlack(), Box(2, 3, 2, 3), "*"));
}

TEST(BasicShapes, FillTinyCircle) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(FilledCircle::ByRadius(2, 3, 1, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          "  *  "
                                          " *** "
                                          "  *  "
                                          "     "));
}

TEST(BasicShapes, DrawTinyCircle) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Circle::ByRadius(2, 3, 1, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          "  *  "
                                          " * * "
                                          "  *  "
                                          "     "));
}

TEST(BasicShapes, FillSmallCircle) {
  FakeOffscreen<Rgb565> test_screen(7, 9, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(FilledCircle::ByRadius(3, 4, 3, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 7, 9,
                                          "       "
                                          "  ***  "
                                          " ***** "
                                          "*******"
                                          "*******"
                                          "*******"
                                          " ***** "
                                          "  ***  "
                                          "       "));
}

TEST(BasicShapes, FillSmallCircleBgRectangle) {
  FakeOffscreen<Argb4444> test_screen(7, 9, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setBackgroundColor(Color(0xFF707070));
    dc.draw(ForceFillRect(FilledCircle::ByRadius(3, 4, 3, color::White)));
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 7, 9,
                                          "       "
                                          "66***66"
                                          "6*****6"
                                          "*******"
                                          "*******"
                                          "*******"
                                          "6*****6"
                                          "66***66"
                                          "       "));
}

TEST(BasicShapes, FillSmallRoundRectBgRectangle) {
  FakeOffscreen<Argb4444> test_screen(7, 9, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setBackgroundColor(Color(0xFF707070));
    dc.draw(ForceFillRect(FilledRoundRect(0, 1, 5, 7, 3, color::White)));
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 7, 9,
                                          "       "
                                          "66**66 "
                                          "6****6 "
                                          "****** "
                                          "****** "
                                          "****** "
                                          "6****6 "
                                          "66**66 "
                                          "       "));
}

TEST(BasicShapes, FillCircleOutOfBounds) {
  FakeOffscreen<Rgb565> test_screen(7, 9, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(FilledCircle::ByRadius(1, 2, 5, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 7, 9,
                                          "****** "
                                          "*******"
                                          "*******"
                                          "*******"
                                          "****** "
                                          "****** "
                                          "*****  "
                                          "***    "
                                          "       "));
}

TEST(BasicShapes, DrawCircleOutOfBounds) {
  FakeOffscreen<Rgb565> test_screen(7, 9, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Circle::ByRadius(1, 2, 5, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 7, 9,
                                          "     * "
                                          "      *"
                                          "      *"
                                          "      *"
                                          "     * "
                                          "     * "
                                          "   **  "
                                          "***    "
                                          "       "));
}

TEST(BasicShapes, DrawHLine) {
  FakeOffscreen<Rgb565> test_screen(5, 4, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Line(1, 2, 3, 2, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 4,
                                          "     "
                                          "     "
                                          " *** "
                                          "     "));
}

TEST(BasicShapes, DrawVLine) {
  FakeOffscreen<Rgb565> test_screen(5, 4, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Line(2, 1, 2, 3, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 4,
                                          "     "
                                          "  *  "
                                          "  *  "
                                          "  *  "));
}

TEST(BasicShapes, DrawLine) {
  FakeOffscreen<Rgb565> test_screen(5, 4, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Line(0, 3, 3, 1, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 4,
                                          "     "
                                          "   * "
                                          " **  "
                                          "*    "));
}

TEST(BasicShapes, DrawSteepLine) {
  FakeOffscreen<Rgb565> test_screen(5, 4, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Line(2, 0, 0, 3, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 4,
                                          "  *  "
                                          " *   "
                                          " *   "
                                          "*    "));
}

TEST(BasicShapes, DrawAnotherSteepLine) {
  FakeOffscreen<Rgb565> test_screen(8, 9, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(Line(3, 7, 6, 1, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 8, 9,
                                          "        "
                                          "      * "
                                          "      * "
                                          "     *  "
                                          "     *  "
                                          "    *   "
                                          "    *   "
                                          "   *    "
                                          "        "));
}

}  // namespace roo_display

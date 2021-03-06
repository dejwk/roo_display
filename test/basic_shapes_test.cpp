
#include "roo_display/shape/basic_shapes.h"

#include "roo_display/core/color.h"
#include "testing.h"
// #include "offscreen.h"

using namespace testing;

namespace roo_display {

TEST(BasicShapes, FillRectOutOfBounds) {
  FakeOffscreen<Rgb565> test_screen(2, 3);
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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

TEST(BasicShapes, FillRectTransparent) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Lime);
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
    // Draw the basic shape #1
    dc.draw(FilledRect(1, 2, 2, 3, color::Blue));
    // And overlay a semi-transparent shape #2
    Color c = color::Red;
    c.set_a(0x40);
    dc.setPaintMode(PAINT_MODE_BLEND);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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

TEST(BasicShapes, FillTinyCircle) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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

TEST(BasicShapes, FillCircleOutOfBounds) {
  FakeOffscreen<Rgb565> test_screen(7, 9, color::Black);
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
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
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
    dc.draw(Line(1, 2, 3, 2, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 4,
                                          "     "
                                          "     "
                                          " *** "
                                          "     "));
}

TEST(BasicShapes, DrawLine) {
  FakeOffscreen<Rgb565> test_screen(5, 4, color::Black);
  Display display(&test_screen);
  {
    DrawingContext dc(&display);
    dc.draw(Line(0, 3, 3, 1, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 4,
                                          "     "
                                          "   * "
                                          " **  "
                                          "*    "));
}

}  // namespace roo_display

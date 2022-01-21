
#include "roo_display.h"
#include "roo_display/core/color.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(DrawingContext, DrawSimple) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(SolidRect(1, 2, 3, 4, color::White));
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          " ***      "
                                          " ***      "
                                          " ***      "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(DrawingContext, DrawWithOffset) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(SolidRect(1, 2, 3, 4, color::White), 2, 3);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "   ***    "
                                          "   ***    "
                                          "   ***    "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(DrawingContext, DrawAlignedTopLeft) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(SolidRect(1, 2, 3, 4, color::White), 2, 3, HAlign::Left(),
            VAlign::Top());
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "  ***     "
                                          "  ***     "
                                          "  ***     "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(DrawingContext, DrawAlignedBottom) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(SolidRect(1, 2, 3, 4, color::White), 2, 6, VAlign::Bottom());
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "   ***    "
                                          "   ***    "
                                          "   ***    "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(DrawingContext, DrawTransformed) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setTransform(Transform().scale(2, 3));
    dc.draw(SolidRect(1, 2, 3, 4, color::White), 2, 1, HAlign::Left(),
            VAlign::Top());
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "  ******  "
                                          "          "));
}

}  // namespace roo_display


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

TEST(DrawingContext, DrawSimpleWithBackgroundColor) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    // Opaque gray background.
    dc.setBackground(Color(0xFF777777));
    // Draw rectangle that is white but 50% transparent.
    dc.draw(SolidRect(1, 2, 3, 4, Color(0x77FFFFFF)));
  }
  // The result should be lighter gray.
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          " BBB      "
                                          " BBB      "
                                          " BBB      "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(DrawingContext, DrawSimpleWithBackground) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  auto bg = MakeRasterizable(
      Box(2, 3, 5, 6),
      [](int16_t x, int16_t y) -> Color {
        return (x + y) % 2 ? color::Transparent : Color(0xFF777777);
      },
      TRANSPARENCY_GRADUAL);
  {
    DrawingContext dc(display);
    dc.setBackground(&bg);
    // Draw rectangle that is white but 50% transparent.
    dc.draw(SolidRect(1, 2, 6, 7, Color(0x77FFFFFF)));
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          " 777777   "
                                          " 77B7B7   "
                                          " 7B7B77   "
                                          " 77B7B7   "
                                          " 7B7B77   "
                                          " 777777   "
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
    dc.draw(SolidRect(1, 2, 3, 4, color::White),
            kLeft.shiftBy(2) | kTop.shiftBy(3));
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
    dc.draw(SolidRect(1, 2, 3, 4, color::White),
            kOrigin.shiftBy(2) | kBottom.toTop().shiftBy(6));
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
    dc.setTransformation(Transformation().scale(2, 3));
    dc.draw(SolidRect(1, 2, 3, 4, color::White),
            kLeft.shiftBy(2) | kTop.shiftBy(1));
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

TEST(DrawingContext, DrawPixels) {
  FakeOffscreen<Argb4444> test_screen(10, 7, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.drawPixels([](PixelWriter& w) {
      w.writePixel(4, 1, Color(0xFF222222));
      w.writePixel(3, 2, Color(0xFF222222));
      w.writePixel(4, 2, Color(0xFF333333));
      w.writePixel(5, 2, Color(0xFF222222));
      w.writePixel(2, 3, Color(0xFF222222));
      w.writePixel(3, 3, Color(0xFF333333));
      w.writePixel(4, 3, Color(0xFF444444));
      w.writePixel(5, 3, Color(0xFF333333));
      w.writePixel(6, 3, Color(0xFF222222));
    });
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 7,
                                          "          "
                                          "    2     "
                                          "   232    "
                                          "  23432   "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(DrawingContext, DrawPixelsWithOffset) {
  FakeOffscreen<Argb4444> test_screen(10, 7, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setTransformation(Transformation().translate(1, 2));
    dc.setClipBox(3, 3, 7, 5);
    dc.drawPixels([](PixelWriter& w) {
      w.writePixel(4, 1, Color(0xFF222222));
      w.writePixel(3, 2, Color(0xFF222222));
      w.writePixel(4, 2, Color(0xFF333333));
      w.writePixel(5, 2, Color(0xFF222222));
      w.writePixel(2, 3, Color(0xFF222222));
      w.writePixel(3, 3, Color(0xFF333333));
      w.writePixel(4, 3, Color(0xFF444444));
      w.writePixel(5, 3, Color(0xFF333333));
      w.writePixel(6, 3, Color(0xFF222222));
    });
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 7,
                                          "          "
                                          "          "
                                          "          "
                                          "     2    "
                                          "    232   "
                                          "   23432  "
                                          "          "));
}

TEST(DrawingContext, DrawPixelsWithOffsetScaled) {
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setTransformation(Transformation().scale(1, 2).translate(1, 2));
    dc.drawPixels([](PixelWriter& w) {
      w.writePixel(4, 1, Color(0xFF222222));
      w.writePixel(3, 2, Color(0xFF222222));
      w.writePixel(4, 2, Color(0xFF333333));
      w.writePixel(5, 2, Color(0xFF222222));
      w.writePixel(2, 3, Color(0xFF222222));
      w.writePixel(3, 3, Color(0xFF333333));
      w.writePixel(4, 3, Color(0xFF444444));
      w.writePixel(5, 3, Color(0xFF333333));
      w.writePixel(6, 3, Color(0xFF222222));
    });
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "     2    "
                                          "     2    "
                                          "    232   "
                                          "    232   "
                                          "   23432  "
                                          "   23432  "
                                          "          "));
}

TEST(DrawingContext, ContextOfSurface) {
  class MyLine : public Drawable {
   public:
    Box extents() const override { return Box(0, 0, 5, 0); }

   private:
    void drawTo(const Surface& s) const override {
      s.out().fillRect(PAINT_MODE_REPLACE, extents().translate(s.dx(), s.dy()),
                       color::White);
    }
  };

  class MyCompound : public Drawable {
   public:
    Box extents() const override { return Box(0, 0, 9, 9); }

   private:
    void drawTo(const Surface& s) const override {
      DrawingContext dc(s);
      dc.draw(MyLine());
      dc.draw(MyLine(), 3, 2);
      dc.setTransformation(Transformation().scale(1, 3));
      dc.draw(MyLine(), 3, 4);
    }
  };

  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(MyCompound());
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "******    "
                                          "          "
                                          "   ****** "
                                          "          "
                                          "   ****** "
                                          "   ****** "
                                          "   ****** "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

}  // namespace roo_display

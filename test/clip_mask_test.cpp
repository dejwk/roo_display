
#include "roo_display/filter/clip_mask.h"

#include "roo_display.h"
#include "roo_display/core/color.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

class SimpleRoundMask {
 public:
  template <typename ColorMode>
  void writePixel(PaintMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    static const char mask[] =
        "                "
        "   *******      "
        "  *********     "
        " ***********    "
        "  *********     "
        "   *******      "
        "                ";
    if (Box(0, 0, 15, 6).contains(x, y) && mask[x + y * 16] != '*') return;
    offscreen->writePixel(mode, x, y, color);
  }

  static ClipMaskFilter* Create(DisplayOutput* output) {
    static const uint8_t clip_mask_data[] = {
        0x00, 0x00, 0x1F, 0xC0, 0x3F, 0xE0, 0x7F,
        0xF0, 0x3F, 0xE0, 0x1F, 0xC0, 0x00, 0x00,
    };
    static ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
    return new ClipMaskFilter(output, &mask);
  }
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleRoundMask> RefDevice;
typedef FilteredOutput<Grayscale4, SimpleRoundMask> TestDevice;

TEST(ClipMask, SimpleTests) {
  TestFillRects<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestFillHLines<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestFillVLines<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestFillDegeneratePixels<TestDevice, RefDevice>(PAINT_MODE_REPLACE,
                                                  Orientation());
  TestFillPixels<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());

  TestWriteRects<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestWriteHLines<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestWriteVLines<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestWriteDegeneratePixels<TestDevice, RefDevice>(PAINT_MODE_REPLACE,
                                                   Orientation());
  TestWritePixels<TestDevice, RefDevice>(PAINT_MODE_REPLACE, Orientation());
  TestWritePixelsSnake<TestDevice, RefDevice>(PAINT_MODE_REPLACE,
                                              Orientation());
  TestWriteRectWindowSimple<TestDevice, RefDevice>(PAINT_MODE_REPLACE,
                                                   Orientation());
}

TEST(ClipMask, StressTests) {
  TestWritePixelsStress<TestDevice, RefDevice>(PAINT_MODE_REPLACE,
                                               Orientation());
  TestWriteRectWindowStress<TestDevice, RefDevice>(PAINT_MODE_REPLACE,
                                                   Orientation());
}

TEST(ClipMask, ClipMaskWrite) {
  FakeOffscreen<Rgb565> test_screen(16, 7);
  Display display(&test_screen);
  const uint8_t clip_mask_data[] = {
      0x00, 0x00, 0x1F, 0xC0, 0x3F, 0xE0, 0x7F,
      0xF0, 0x3F, 0xE0, 0x1F, 0xC0, 0x00, 0x00,
  };
  ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
  {
    DrawingContext dc(&display);
    dc.setClipMask(&mask);
    dc.draw(MakeTestStreamable(WhiteOnBlack(), 14, 6,
                               "**************"
                               "          ****"
                               "**************"
                               "**************"
                               "***           "
                               "***           "
                               "**************"),
            1, 0);
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 16, 7,
                                          "                "
                                          "                "
                                          "  *********     "
                                          " ***********    "
                                          "  **            "
                                          "   *            "
                                          "                "));
}

TEST(ClipMask, ClipMaskStreamableSemiTransparent) {
  FakeOffscreen<Rgb565> test_screen(16, 7);
  Display display(&test_screen);
  const uint8_t clip_mask_data[] = {
      0x00, 0x00, 0x1F, 0xC0, 0x3F, 0xE0, 0x7F,
      0xF0, 0x3F, 0xE0, 0x1F, 0xC0, 0x00, 0x00,
  };
  ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
  {
    DrawingContext dc(&display);
    dc.setClipMask(&mask);
    dc.draw(MakeTestStreamable(Alpha4(color::White), 14, 6,
                               "**************"
                               "          ****"
                               "**************"
                               "**************"
                               "***           "
                               "***           "
                               "**************"),
            1, 0);
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 16, 7,
                                          "                "
                                          "                "
                                          "  *********     "
                                          " ***********    "
                                          "  **            "
                                          "   *            "
                                          "                "));
}

}  // namespace roo_display

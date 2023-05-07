
#include "roo_display/filter/clip_mask.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

class SimpleRoundMask {
 public:
  SimpleRoundMask(Box extents) {}

  template <typename ColorMode>
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    static const char mask[] =
        "                "
        "   *******      "
        "  *********     "
        " ***********    "
        "  *********     "
        "   *******      "
        "                ";
    if (Box(1, 2, 16, 8).contains(x, y) && mask[x - 1 + (y - 2) * 16] == '*')
      return;
    offscreen->writePixel(mode, x, y, color);
  }

  static ClipMaskFilter* Create(DisplayOutput& output, Box extents) {
    static const uint8_t clip_mask_data[] = {
        0x00, 0x00, 0x1F, 0xC0, 0x3F, 0xE0, 0x7F,
        0xF0, 0x3F, 0xE0, 0x1F, 0xC0, 0x00, 0x00,
    };
    static ClipMask mask(clip_mask_data, Box(1, 2, 16, 8));
    return new ClipMaskFilter(output, &mask);
  }
};

class LargeMask {
 public:
  LargeMask(Box extents) {}

  template <typename ColorMode>
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    static const char mask[] =
        "                                "
        "   ***********************      "
        "  *************************     "
        "  *************************     "
        " ***************************    "
        " ***************************    "
        " ***************************    "
        "  *************************     "
        "  *************************     "
        "   ***********************      "
        "   ***********************      "
        "   ***********************      "
        "  *************************     "
        "  *************************     "
        " ***************************    "
        " ***************************    "
        " ***************************    "
        "  *************************     "
        "  *************************     "
        "   ***********************      "
        "                                ";
    if (Box(1, 2, 32, 22).contains(x, y) && mask[x - 1 + (y - 2) * 32] == '*')
      return;
    offscreen->writePixel(mode, x, y, color);
  }

  static ClipMaskFilter* Create(DisplayOutput& output, Box extents) {
    static const uint8_t clip_mask_data[] = {
        0b00000000, 0b00000000, 0b00000000, 0b00000000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00000000, 0b00000000, 0b00000000, 0b00000000,  // NOFORMAT
    };
    static ClipMask mask(clip_mask_data, Box(1, 2, 32, 22));
    return new ClipMaskFilter(output, &mask);
  }
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleRoundMask> RefDeviceSimple;
typedef FilteredOutput<Grayscale4, SimpleRoundMask> TestDeviceSimple;

TEST(ClipMask, SimpleTests) {
  TestFillRects<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                   Orientation());
  TestFillHLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());
  TestFillVLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());
  TestFillDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
  TestFillPixels<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());

  TestWriteRects<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                    Orientation());
  TestWriteHLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                     Orientation());
  TestWriteVLines<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                     Orientation());
  TestWriteDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
  TestWritePixels<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                     Orientation());
  TestWritePixelsSnake<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                          Orientation());
  TestWriteRectWindowSimple<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
}

TEST(ClipMask, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                           Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
}

typedef FakeFilteringOffscreen<Grayscale4, LargeMask> RefDeviceLarge;
typedef FilteredOutput<Grayscale4, LargeMask> TestDeviceLarge;

TEST(ClipMask, SimpleLargeTests) {
  TestFillRects<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                 Orientation());
  TestFillHLines<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                  Orientation());
  TestFillVLines<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                  Orientation());
  TestFillDegeneratePixels<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                            Orientation());
  TestFillPixels<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                  Orientation());

  TestWriteRects<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                  Orientation());
  TestWriteHLines<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                   Orientation());
  TestWriteVLines<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                   Orientation());
  TestWriteDegeneratePixels<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                             Orientation());
  TestWritePixels<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                   Orientation());
  TestWritePixelsSnake<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                        Orientation());
  TestWriteRectWindowSimple<TestDeviceLarge, RefDeviceLarge>(BLENDING_MODE_SOURCE,
                                                             Orientation());
}

TEST(ClipMask, ClipMaskWrite) {
  FakeOffscreen<Rgb565> test_screen(16, 7);
  Display display(test_screen);
  const uint8_t clip_mask_data[] = {
      0xFF, 0xFF, 0xE0, 0x3F, 0xC0, 0x1F, 0x80,
      0x0F, 0xC0, 0x1F, 0xE0, 0x3F, 0xFF, 0xFF,
  };
  ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
  {
    DrawingContext dc(display);
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
  Display display(test_screen);
  const uint8_t clip_mask_data[] = {
      0xFF, 0xFF, 0xE0, 0x3F, 0xC0, 0x1F, 0x80,
      0x0F, 0xC0, 0x1F, 0xE0, 0x3F, 0xFF, 0xFF,
  };
  ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
  {
    DrawingContext dc(display);
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

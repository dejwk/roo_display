
#include "roo_display.h"
#include "roo_display/core/color.h"
#include "roo_display/filter/foreground.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

static const char mask[] =
    "                "
    "   1234321      "
    "  123454321     "
    " 12345654321    "
    "  345676543     "
    "   5678765      "
    "                ";

class SimpleRoundFg {
 public:
  SimpleRoundFg(Box extents) {}
  template <typename ColorMode>
  void writePixel(PaintMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    Color fgcolor = color::Transparent;
    if (Box(1, 2, 16, 8).contains(x, y)) {
      const char fg = mask[x - 1 + (y - 2) * 16];
      if (fg != ' ') {
        uint8_t grey = (fg - '0') * 0x11;
        fgcolor = Color(grey, grey, grey);
      }
    }
    offscreen->writePixel(mode, x, y, AlphaBlend(color, fgcolor));
  }

  static DisplayOutput* Create(DisplayOutput& output, Box extents) {
    Box bounds(1, 2, 16, 8);
    static auto raster = MakeRasterizable(
        bounds,
        [bounds](int16_t x, int16_t y) -> Color {
          EXPECT_TRUE(bounds.contains(x, y))
              << "Out-of-bounds read: (" << x << ", " << y
              << "), while bounds = " << bounds;
          const char fg = mask[x - 1 + (y - 2) * 16];
          if (fg == ' ') return color::Transparent;
          uint8_t grey = (fg - '0') * 0x11;
          return Color(grey, grey, grey);
        },
        TRANSPARENCY_BINARY);
    return new ForegroundFilter(output, &raster);
  }
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleRoundFg> RefDeviceSimple;
typedef FilteredOutput<Grayscale4, SimpleRoundFg> TestDeviceSimple;

TEST(Background, SimpleTests) {
  TestFillRects<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                   Orientation());
  TestFillHLines<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                    Orientation());
  TestFillVLines<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                    Orientation());
  TestFillDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      PAINT_MODE_REPLACE, Orientation());
  TestFillPixels<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                    Orientation());

  TestWriteRects<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                    Orientation());
  TestWriteHLines<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                     Orientation());
  TestWriteVLines<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                     Orientation());
  TestWriteDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      PAINT_MODE_REPLACE, Orientation());
  TestWritePixels<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                     Orientation());
  TestWritePixelsSnake<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                          Orientation());
  TestWriteRectWindowSimple<TestDeviceSimple, RefDeviceSimple>(
      PAINT_MODE_REPLACE, Orientation());
}

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(PAINT_MODE_REPLACE,
                                                           Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      PAINT_MODE_REPLACE, Orientation());
}

}  // namespace roo_display

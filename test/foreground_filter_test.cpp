
#include "roo_display.h"
#include "roo_display/color/color.h"
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
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
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
        TransparencyMode::kCrude);
    return new ForegroundFilter(output, &raster);
  }
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleRoundFg> RefDeviceSimple;
typedef FilteredOutput<Grayscale4, SimpleRoundFg> TestDeviceSimple;

TEST(Background, SimpleTests) {
  TestFillRects<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                   Orientation());
  TestFillHLines<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                    Orientation());
  TestFillVLines<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                    Orientation());
  TestFillDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
  TestFillPixels<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                    Orientation());

  TestWriteRects<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                    Orientation());
  TestWriteHLines<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                     Orientation());
  TestWriteVLines<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                     Orientation());
  TestWriteDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
  TestWritePixels<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                     Orientation());
  TestWritePixelsSnake<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                          Orientation());
  TestWriteRectWindowSimple<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
}

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
}

}  // namespace roo_display

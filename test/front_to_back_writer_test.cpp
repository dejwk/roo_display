
#include "roo_display/filter/front_to_back_writer.h"

#include "roo_display.h"
#include "roo_display/core/color.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

class SimpleTestable {
 public:
  SimpleTestable(Box extents)
      : bounds_(extents), mask_(new bool[extents.area()]) {
    std::fill(&mask_[0], &mask_[extents.area()], false);
  }

  template <typename ColorMode>
  void writePixel(PaintMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    // if (!bounds_.contains(x, y)) {
    //   offscreen->writePixel(mode, x, y, color);
    //   return;
    // }
    bool& val =
        mask_[x - bounds_.xMin() + (y - bounds_.yMin()) * bounds_.width()];
    if (val) return;
    offscreen->writePixel(mode, x, y, color);
    val = true;
  }

  static DisplayOutput* Create(DisplayOutput& output, Box bounds) {
    return new FrontToBackWriter(output, bounds);
  }

 private:
  Box bounds_;
  std::unique_ptr<bool[]> mask_;
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleTestable> RefDeviceSimple;
typedef FilteredOutput<Grayscale4, SimpleTestable> TestDeviceSimple;

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

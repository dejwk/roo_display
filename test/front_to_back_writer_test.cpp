
#include "roo_display/filter/front_to_back_writer.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
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
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
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

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(BLENDING_MODE_SOURCE,
                                                           Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BLENDING_MODE_SOURCE, Orientation());
}

}  // namespace roo_display

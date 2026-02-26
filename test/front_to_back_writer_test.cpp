
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

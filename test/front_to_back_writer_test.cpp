
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
  TestFillRects<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                   Orientation());
  TestFillHLines<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                    Orientation());
  TestFillVLines<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                    Orientation());
  TestFillDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                              Orientation());
  TestFillPixels<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                    Orientation());

  TestWriteRects<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                    Orientation());
  TestWriteHLines<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                     Orientation());
  TestWriteVLines<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                     Orientation());
  TestWriteDegeneratePixels<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                               Orientation());
  TestWritePixels<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                     Orientation());
  TestWritePixelsSnake<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                          Orientation());
  TestWriteRectWindowSimple<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                               Orientation());
}

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                           Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(kBlendingSource,
                                                               Orientation());
}

}  // namespace roo_display

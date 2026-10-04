
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
  TestDrawDirectRect<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                        Orientation());
}

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
}

namespace {
class FrontCountingOutput : public FakeOffscreen<Argb8888> {
 public:
  using FakeOffscreen::FakeOffscreen;

  void writePixels(BlendingMode mode, Color* colors, int16_t* x, int16_t* y,
                   uint16_t count) override {
    ++writes;
    FakeOffscreen::writePixels(mode, colors, x, y, count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t count) override {
    ++fills;
    FakeOffscreen::fillPixels(mode, color, x, y, count);
  }

  int writes = 0;
  int fills = 0;
};
}  // namespace

// Verifies compaction batches visible pixels, immediately marks duplicates, and
// keeps mask coordinates relative to a nonzero origin across successive calls.
TEST(FrontToBackWriter, BatchesUniquePixelsAndPreservesFirstWrite) {
  FrontCountingOutput output(20, 8);
  FrontToBackWriter filter(output, Box(3, 2, 18, 6));
  int16_t x[] = {3, 4, 3, 5, 4, 6};
  int16_t y[] = {2, 2, 2, 3, 2, 4};
  Color colors[] = {color::Red,   color::Blue,  color::Green,
                    color::White, color::Green, color::Blue};
  filter.writePixels(BlendingMode::kSource, colors, x, y, 6);
  EXPECT_EQ(output.writes, 1);
  EXPECT_EQ(output.fills, 0);
  EXPECT_EQ(output.buffer()[2 * 20 + 3], color::Red);
  EXPECT_EQ(output.buffer()[2 * 20 + 4], color::Blue);
  int16_t fill_x[] = {3, 7, 7, 8};
  int16_t fill_y[] = {2, 3, 3, 3};
  filter.fillPixels(BlendingMode::kSource, color::Green, fill_x, fill_y, 4);
  EXPECT_EQ(output.fills, 1);
  EXPECT_EQ(output.buffer()[2 * 20 + 3], color::Red);
  EXPECT_EQ(output.buffer()[3 * 20 + 7], color::Green);
  EXPECT_EQ(output.buffer()[3 * 20 + 8], color::Green);
  filter.fillPixels(BlendingMode::kSource, color::Blue, fill_x, fill_y, 2);
  EXPECT_EQ(output.fills, 1);
}

}  // namespace roo_display

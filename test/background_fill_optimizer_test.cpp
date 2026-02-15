#include "roo_display/filter/background_fill_optimizer.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/shape/basic.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

// Test device: Wraps FakeOffscreen with BackgroundFillOptimizer.
template <typename ColorMode>
class OptimizedDevice : public DisplayDevice {
 public:
  OptimizedDevice(int16_t width, int16_t height,
                  Color prefilled = color::Transparent)
      : DisplayDevice(width, height),
        device_(width, height, prefilled, ColorMode()),
        optimizer_(device_) {
    optimizer_.setPalette({prefilled}, prefilled);
  }

  void orientationUpdated() override {
    optimizer_.setOrientation(orientation());
  }

  void setPalette(std::initializer_list<Color> palette,
                  Color prefilled = color::Transparent) {
    optimizer_.setPalette(palette, prefilled);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    optimizer_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    optimizer_.write(color, pixel_count);
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    optimizer_.writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    optimizer_.fillPixels(mode, color, x, y, pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    optimizer_.writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    optimizer_.fillRects(mode, color, x0, y0, x1, y1, count);
  }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0,
                      int16_t dst_y0) override {
    optimizer_.drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                              src_y1, dst_x0, dst_y0);
  }

  const ColorFormat& getColorFormat() const override {
    return optimizer_.getColorFormat();
  }

  const FakeOffscreen<ColorMode>& device() const { return device_; }

  const BackgroundFillOptimizer::FrameBuffer& buffer() const {
    return optimizer_.buffer();
  }

 private:
  FakeOffscreen<ColorMode> device_;
  BackgroundFillOptimizerDevice optimizer_;
};

// Raster extractor for OptimizedDevice.
template <typename ColorMode>
TestColorStreamable<ColorMode> RasterOf(
    const OptimizedDevice<ColorMode>& device) {
  return RasterOf(device.device());
}

// Simplified test screen type.
using TestScreen =
    TestDisplayDevice<OptimizedDevice<Rgb565>, FakeOffscreen<Rgb565>>;

// Helper to create a Raster for frame buffer content verification.
// The frame buffer stores palette indices as nibbles (4-bit values) in a
// coarse grid. For a WxH display with 4x4 window size, the buffer is
// (W/4)x(H/4) nibbles, stored MSB-first (even x in high nibble).
template <typename ColorMode>
auto FrameBufferOf(const OptimizedDevice<ColorMode>& device) {
  const auto& buffer = device.buffer();
  const auto& mask = buffer.mask();
  return Raster<const roo::byte*, Grayscale4>(mask.width(), mask.height(),
                                              mask.buffer(), Grayscale4());
}

template <typename ColorMode>
void ExpectFrameBufferMatches(const OptimizedDevice<ColorMode>& device,
                              const std::string& expected) {
  const auto& mask = device.buffer().mask();
  EXPECT_THAT(FrameBufferOf(device), MatchesContent(Grayscale4(), mask.width(),
                                                    mask.height(), expected));
}

#define EXPECT_FRAMEBUFFER_MATCHES(screen, ...) \
  ExpectFrameBufferMatches((screen).test(), ##__VA_ARGS__)

// Basic tests for FrameBuffer.

TEST(BackgroundFillOptimizer, FrameBufferConstruction) {
  // Verifies frame buffer construction with owned and external storage.
  // Test dynamic allocation.
  BackgroundFillOptimizer::FrameBuffer buffer1(320, 240);
  EXPECT_EQ(buffer1.palette_size(), 0);

  // Test with provided storage.
  const int16_t width = 64;
  const int16_t height = 48;
  size_t size =
      BackgroundFillOptimizer::FrameBuffer::SizeForDimensions(width, height);
  std::unique_ptr<roo::byte[]> storage(new roo::byte[size]);
  BackgroundFillOptimizer::FrameBuffer buffer2(width, height, storage.get());
  EXPECT_EQ(buffer2.palette_size(), 0);
}

TEST(BackgroundFillOptimizer, FrameBufferPaletteSetup) {
  // Verifies palette assignment APIs and resulting stored palette entries.
  BackgroundFillOptimizer::FrameBuffer buffer(64, 48);

  // Set palette using array.
  Color palette[] = {color::Red, color::Green, color::Blue};
  buffer.setPalette(palette, 3);
  EXPECT_EQ(buffer.palette_size(), 3);
  EXPECT_EQ(buffer.palette()[0], color::Red);
  EXPECT_EQ(buffer.palette()[1], color::Green);
  EXPECT_EQ(buffer.palette()[2], color::Blue);

  // Set palette using initializer list.
  buffer.setPalette({color::White, color::Black});
  EXPECT_EQ(buffer.palette_size(), 2);
  EXPECT_EQ(buffer.palette()[0], color::White);
  EXPECT_EQ(buffer.palette()[1], color::Black);
}

TEST(BackgroundFillOptimizer, FrameBufferSizeCalculation) {
  // Verifies byte size calculation for representative display dimensions.
  // Test exact size calculations for various dimensions.
  EXPECT_EQ(BackgroundFillOptimizer::FrameBuffer::SizeForDimensions(320, 240),
            2400u);
  EXPECT_EQ(BackgroundFillOptimizer::FrameBuffer::SizeForDimensions(480, 320),
            4800u);
  EXPECT_EQ(BackgroundFillOptimizer::FrameBuffer::SizeForDimensions(128, 64),
            256u);
  EXPECT_EQ(BackgroundFillOptimizer::FrameBuffer::SizeForDimensions(240, 160),
            1200u);
}

// Tests for optimization behavior.

TEST(BackgroundFillOptimizer, SimpleFillRect) {
  // Verifies a basic background-colored filled rectangle is fully optimized
  // away.
  TestScreen screen(48, 32, color::White);
  Display display(screen);
  DrawingContext dc(display);

  // Same-as-background fill should be skipped by optimizer.
  dc.draw(FilledRect(10, 10, 20, 20, color::White));
  EXPECT_EQ(screen.test().device().pixelDrawCount(), 0u);
  EXPECT_EQ(screen.refc().pixelDrawCount(), 121u);

  EXPECT_CONSISTENT(screen);

  // Verify frame buffer dimensions: 48x32 pixels -> 12x8 windows.
  const auto& buffer = screen.test().buffer();
  EXPECT_EQ(buffer.mask().width(), 12);
  EXPECT_EQ(buffer.mask().height(), 8);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111");
}

TEST(BackgroundFillOptimizer, RedundantFillOptimization) {
  // Verifies repeated identical background fills preserve correctness and mask
  // state.
  TestScreen screen(48, 32, color::White);
  screen.test().setPalette({color::White, color::Purple}, color::White);
  Display display(screen);
  DrawingContext dc(display);

  // First fill.
  dc.draw(FilledRect(8, 8, 23, 23, color::Purple));

  // This should invalidate some regions.
  int16_t sx[] = {5, 15, 25, 35, 45};
  int16_t sy[] = {6, 10, 14, 18, 22};
  screen.fillPixels(BLENDING_MODE_SOURCE, color::Yellow, sx, sy, 5);

  const uint64_t test_before_second_fill =
      screen.test().device().pixelDrawCount();
  const uint64_t ref_before_second_fill = screen.refc().pixelDrawCount();

  // Second fill - optimizer should mostly optimize this away, except
  // for areas invalidated by fillPixels.
  dc.draw(FilledRect(8, 8, 23, 23, color::Purple));

  const uint64_t test_after_second_fill =
      screen.test().device().pixelDrawCount();
  const uint64_t ref_after_second_fill = screen.refc().pixelDrawCount();

  // Repeated same-color background fill redraws only invalidated blocks.
  // Specifically, the {15,5} point invalidates the 4x4 block covering (12-15,
  // 8-11), which is the only part that needs to be redrawn.
  EXPECT_EQ(test_after_second_fill - test_before_second_fill, 16u);
  // Reference still writes the full 16x16 rectangle.
  EXPECT_EQ(ref_after_second_fill - ref_before_second_fill, 256u);
  EXPECT_LT(test_after_second_fill, ref_after_second_fill);

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "1 1111111111"
                             "112222111111"
                             "112222 11111"
                             "11222211 111"
                             "11222211111 "
                             "111111111111"
                             "111111111111");
}

TEST(BackgroundFillOptimizer, NonPaletteColorInvalidatesMask) {
  // Verifies non-palette draws invalidate mask cells and subsequent bg fill
  // behavior.
  TestScreen screen(48, 32, color::White);
  Display display(screen);
  DrawingContext dc(display);

  // Draw a foreground color (not in palette).
  dc.draw(FilledRect(10, 10, 20, 20, color::Yellow));
  EXPECT_EQ(121, screen.test().device().pixelDrawCount());

  // Now fill with background color - should still write since mask invalidated.
  dc.draw(FilledRect(10, 10, 20, 20, color::White));
  EXPECT_EQ(242, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);

  // Since the border of the rectangle is not aligned to the 4x4 grid, we expect
  // to see borders be non-background.
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "11    111111"
                             "11 11 111111"
                             "11 11 111111"
                             "11    111111"
                             "111111111111"
                             "111111111111");
}

TEST(BackgroundFillOptimizer, MultipleColors) {
  // Verifies palette-index tracking when drawing several palette colors.
  TestScreen screen(48, 32, color::Red);
  Display display(screen);
  DrawingContext dc(display);

  // Fill different areas with different colors.
  screen.test().setPalette({color::Red, color::Green, color::Blue}, color::Red);
  dc.draw(FilledRect(0, 0, 12, 10, color::Red));
  dc.draw(FilledRect(16, 12, 28, 24, color::Green));
  dc.draw(FilledRect(32, 6, 44, 18, color::Blue));

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "11111111    "
                             "11111111333 "
                             "1111222 333 "
                             "1111222     "
                             "1111222 1111"
                             "1111    1111"
                             "111111111111");
  EXPECT_EQ(338u, screen.test().device().pixelDrawCount());
}

TEST(BackgroundFillOptimizer, FillPixels) {
  // Verifies sparse background pixel fills and resulting mask updates.
  TestScreen screen(48, 32, color::White);

  int16_t x[] = {5, 15, 25, 35, 45};
  int16_t y[] = {6, 10, 14, 18, 22};
  screen.fillPixels(BLENDING_MODE_SOURCE, color::White, x, y, 5);
  int16_t sx[] = {5, 15, 25, 35, 45};
  int16_t sy[] = {6, 10, 14, 18, 22};
  screen.fillPixels(BLENDING_MODE_SOURCE, color::Yellow, sx, sy, 5);

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "1 1111111111"
                             "111 11111111"
                             "111111 11111"
                             "11111111 111"
                             "11111111111 "
                             "111111111111"
                             "111111111111");
  EXPECT_EQ(5u, screen.test().device().pixelDrawCount());
}

TEST(BackgroundFillOptimizer, WritePixels) {
  // Verifies sparse pixel writes with mixed palette/non-palette colors.
  TestScreen screen(48, 32, color::White);

  int16_t x[] = {5, 15, 25, 35, 45};
  int16_t y[] = {6, 10, 14, 18, 22};
  Color colors[] = {color::White, color::Red, color::White, color::Blue,
                    color::White};
  screen.writePixels(BLENDING_MODE_SOURCE, colors, x, y, 5);

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "111 11111111"
                             "111111111111"
                             "11111111 111"
                             "111111111111"
                             "111111111111"
                             "111111111111");
  EXPECT_EQ(2u, screen.test().device().pixelDrawCount());
}

TEST(BackgroundFillOptimizer, WriteRects) {
  // Verifies rectangle writes with mixed colors keep optimizer/reference
  // consistent.
  TestScreen screen(48, 32, color::White);
  Display display(screen);
  DrawingContext dc(display);

  dc.draw(FilledRect(4, 4, 14, 12, color::White));
  EXPECT_EQ(0u, screen.test().device().pixelDrawCount());
  dc.draw(FilledRect(26, 18, 38, 30, color::Green));
  EXPECT_EQ(169, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111    11"
                             "111111    11"
                             "111111    11"
                             "111111    11");
}

TEST(BackgroundFillOptimizer, FillRects) {
  // Verifies multiple background rectangle fills across separate regions.
  TestScreen screen(48, 32, color::White);
  screen.test().setPalette({color::White, color::Green}, color::White);
  Display display(screen);
  DrawingContext dc(display);

  dc.draw(FilledRect(4, 4, 16, 10, color::Green));
  dc.draw(FilledRect(18, 12, 30, 20, color::Green));
  dc.draw(FilledRect(8, 20, 20, 28, color::Green));
  EXPECT_EQ(325, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "1222 1111111"
                             "1    1111111"
                             "1111 22 1111"
                             "1111 22 1111"
                             "11222   1111"
                             "11222 111111"
                             "11    111111");
}

TEST(BackgroundFillOptimizer, FillRectBgWithNonPaletteOverlay) {
  // Verifies full bg fill, non-palette overlay, and bg refill sequence.
  TestScreen screen(48, 32, color::White);
  Display display(screen);
  DrawingContext dc(display);

  // First, fill large background area.
  dc.draw(FilledRect(0, 0, 47, 31, color::White));
  EXPECT_EQ(0, screen.test().device().pixelDrawCount());

  // Draw non-palette color, invalidating mask.
  dc.draw(FilledRect(12, 8, 36, 24, color::Yellow));
  EXPECT_EQ(425, screen.test().device().pixelDrawCount());

  // Fill background again in same region - should still match reference.
  dc.draw(FilledRect(0, 0, 47, 31, color::White));
  EXPECT_EQ(985, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111");
}

TEST(BackgroundFillOptimizer, RepeatedBackgroundFillPattern) {
  // Verifies a mixed multi-rectangle palette pattern with repeated bg writes.
  TestScreen screen(48, 32, color::Black);
  Display display(screen);
  DrawingContext dc(display);

  // Create a complex pattern with multiple palette colors and fills.
  screen.test().setPalette({color::Black, color::Red, color::Blue},
                           color::Black);
  // Multiple fills with background color should trigger optimization.
  dc.draw(FilledRect(4, 4, 12, 12, color::Black));
  dc.draw(FilledRect(16, 8, 24, 16, color::Red));
  dc.draw(FilledRect(28, 12, 36, 20, color::Blue));
  dc.draw(FilledRect(8, 20, 16, 28, color::Black));
  dc.draw(FilledRect(20, 24, 32, 30, color::Red));
  EXPECT_EQ(253, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "111122 11111"
                             "111122 33 11"
                             "1111   33 11"
                             "1111111   11"
                             "11111222 111"
                             "11111    111");
}

TEST(BackgroundFillOptimizer, AlternatingColors) {
  // Verifies alternating palette/non-palette rectangles and mask transitions.
  TestScreen screen(48, 32, color::Red);
  Display display(screen);
  DrawingContext dc(display);

  // Alternate between palette and non-palette colors in same region.
  screen.test().setPalette({color::Red, color::Blue}, color::Red);
  // Mix of colors to test mask invalidation behavior.
  dc.draw(FilledRect(6, 8, 14, 14, color::Yellow));
  dc.draw(FilledRect(18, 8, 26, 14, color::Red));
  dc.draw(FilledRect(30, 8, 38, 14, color::Green));
  dc.draw(FilledRect(6, 20, 14, 30, color::White));
  dc.draw(FilledRect(18, 20, 26, 30, color::Red));
  dc.draw(FilledRect(30, 20, 38, 30, color::Blue));
  EXPECT_EQ(324, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "1   111   11"
                             "1   111   11"
                             "111111111111"
                             "1   111 2 11"
                             "1   111 2 11"
                             "1   111   11");
}

namespace {

Offscreen<Rgb888> MakeCirclePatternSourceBuffer(Color bg, Color big_circle,
                                                Color small_circle) {
  constexpr int kWidth = 50;
  constexpr int kHeight = 50;

  // Draw the source pattern using shape primitives. Using offscreen w/o
  // transparency to force FILL_MODE_RECTANGLE, but not using Rgb565 to avoid
  // the direct draw path.
  Offscreen<Rgb888> source(kWidth, kHeight, bg, Rgb888());
  DrawingContext dc(source);

  // Big circle: diameter 48 in a 50x50 image (1-pixel border all around).
  dc.draw(FilledCircle::ByExtents(1, 1, 48, big_circle));

  // Small circle near the edge of the big circle.
  dc.draw(FilledCircle::ByRadius(46, 24, 3, small_circle));

  return source;
}

Offscreen<Rgb565> MakeCirclePatternSourceBufferRgb565(Color bg,
                                                      Color big_circle,
                                                      Color small_circle) {
  constexpr int kWidth = 50;
  constexpr int kHeight = 50;

  // Same pattern as the Rgb888 helper, but in Rgb565 to allow the raster
  // direct-draw path to match the destination color format.
  Offscreen<Rgb565> source(kWidth, kHeight, bg, Rgb565());
  DrawingContext dc(source);

  dc.draw(FilledCircle::ByExtents(1, 1, 48, big_circle));
  dc.draw(FilledCircle::ByRadius(46, 24, 3, small_circle));

  return source;
}

void DrawClippedRectFromSource(DrawingContext& dc, const Drawable& source,
                               int16_t sx0, int16_t sy0, int16_t sx1,
                               int16_t sy1, int16_t dx0, int16_t dy0) {
  const int16_t width = sx1 - sx0 + 1;
  const int16_t height = sy1 - sy0 + 1;
  const int16_t dx1 = dx0 + width - 1;
  const int16_t dy1 = dy0 + height - 1;

  const Box previous_clip = dc.getClipBox();
  dc.setClipBox(dx0, dy0, dx1, dy1);
  dc.draw(source, dx0 - sx0, dy0 - sy0);
  dc.setClipBox(previous_clip);
}

}  // namespace

TEST(BackgroundFillOptimizer, WriteSubRectsFromPatternSource) {
  // Verifies write() path via setAddress/write with mixed palette and
  // non-palette source pixels.
  constexpr Color kBg = color::White;
  constexpr Color kBig = color::Blue;
  constexpr Color kSmall = color::Yellow;

  TestScreen screen(64, 64, kBg);
  screen.test().setPalette({kBg, kBig}, kBg);
  auto source_image = MakeCirclePatternSourceBuffer(kBg, kBig, kSmall);
  Display display(screen);
  DrawingContext dc(display);

  // Copy varied source sub-rectangles by drawing with clipped context:
  // - 8x8 (64 px), aligned to 4x4 grid blocks on destination.
  // - 1x13 thin strip (1-pixel wide).
  // - 17x5 non-square rectangle.
  DrawClippedRectFromSource(dc, source_image, 4, 4, 11, 11, 8, 8);     // 64
  DrawClippedRectFromSource(dc, source_image, 46, 18, 46, 30, 41, 26);  // 13
  DrawClippedRectFromSource(dc, source_image, 14, 22, 30, 26, 20, 40);  // 85

  EXPECT_CONSISTENT(screen);

  // Reference writes all streamed pixels, optimizer should skip many
  // background-over-background writes.
  const uint64_t ref_count = screen.refc().pixelDrawCount();
  const uint64_t test_count = screen.test().device().pixelDrawCount();
  EXPECT_EQ(ref_count, 162u);
  EXPECT_GT(test_count, 0u);
  EXPECT_LT(test_count, ref_count);
}

TEST(BackgroundFillOptimizer, DrawDirectSubRectsFromRgb565PatternSource) {
  // Verifies drawDirectRect path from Raster when source/destination formats
  // match, using sub-rectangle draws similar to WriteSubRectsFromPatternSource.
  constexpr Color kBg = color::White;
  constexpr Color kBig = color::Blue;
  constexpr Color kSmall = color::Yellow;

  OptimizedDevice<Rgb565> optimized(64, 64, kBg);
  optimized.setPalette({kBg, kBig}, kBg);
  FakeOffscreen<Rgb565> reference(64, 64, kBg, Rgb565());

  auto source_image = MakeCirclePatternSourceBufferRgb565(kBg, kBig, kSmall);

  Display optimized_display(optimized);
  DrawingContext optimized_dc(optimized_display);
  Display reference_display(reference);
  DrawingContext reference_dc(reference_display);

  DrawClippedRectFromSource(optimized_dc, source_image, 4, 4, 11, 11, 8, 8);
  DrawClippedRectFromSource(optimized_dc, source_image, 46, 18, 46, 30, 41,
                            26);
  DrawClippedRectFromSource(optimized_dc, source_image, 14, 22, 30, 26, 20,
                            40);

  DrawClippedRectFromSource(reference_dc, source_image, 4, 4, 11, 11, 8, 8);
  DrawClippedRectFromSource(reference_dc, source_image, 46, 18, 46, 30, 41,
                            26);
  DrawClippedRectFromSource(reference_dc, source_image, 14, 22, 30, 26, 20,
                            40);

  EXPECT_THAT(RasterOf(optimized), MatchesContent(RasterOf(reference)));
}

}  // namespace roo_display

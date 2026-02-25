#include "roo_display/filter/background_fill_optimizer.h"

#include <memory>
#include <random>
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

  void fill(Color color, uint32_t pixel_count) override {
    optimizer_.fill(color, pixel_count);
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
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
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
  // EXPECT_EQ(buffer1.palette_size(), 0);

  // Test with provided storage.
  const int16_t width = 64;
  const int16_t height = 48;
  size_t size =
      BackgroundFillOptimizer::FrameBuffer::SizeForDimensions(width, height);
  std::unique_ptr<roo::byte[]> storage(new roo::byte[size]);
  BackgroundFillOptimizer::FrameBuffer buffer2(width, height, storage.get());
  // EXPECT_EQ(buffer2.palette_size(), 0);
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
  screen.fillPixels(kBlendingSource, color::Yellow, sx, sy, 5);

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
  EXPECT_EQ(121u, screen.test().device().pixelDrawCount());

  // Now fill with background color - should still write since mask invalidated.
  dc.draw(FilledRect(10, 10, 20, 20, color::White));
  EXPECT_EQ(242u, screen.test().device().pixelDrawCount());

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
  screen.fillPixels(kBlendingSource, color::White, x, y, 5);
  int16_t sx[] = {5, 15, 25, 35, 45};
  int16_t sy[] = {6, 10, 14, 18, 22};
  screen.fillPixels(kBlendingSource, color::Yellow, sx, sy, 5);

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
  screen.writePixels(kBlendingSource, colors, x, y, 5);

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
  EXPECT_EQ(169u, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111111111"
                             "111111    11"
                             "111111 22 11"
                             "111111 22 11"
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
  EXPECT_EQ(325u, screen.test().device().pixelDrawCount());

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
  EXPECT_EQ(0u, screen.test().device().pixelDrawCount());

  // Draw non-palette color, invalidating mask.
  dc.draw(FilledRect(12, 8, 36, 24, color::Yellow));
  EXPECT_EQ(425u, screen.test().device().pixelDrawCount());

  // Fill background again in same region - should still match reference.
  dc.draw(FilledRect(0, 0, 47, 31, color::White));
  EXPECT_EQ(985u, screen.test().device().pixelDrawCount());

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
  EXPECT_EQ(253u, screen.test().device().pixelDrawCount());

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
  EXPECT_EQ(324u, screen.test().device().pixelDrawCount());

  EXPECT_CONSISTENT(screen);
  EXPECT_FRAMEBUFFER_MATCHES(screen,
                             "111111111111"
                             "111111111111"
                             "1   111   11"
                             "1   111   11"
                             "111111111111"
                             "1 3 111 2 11"
                             "1 3 111 2 11"
                             "1   111   11");
}

namespace {

class SplitFillDrawable : public Drawable {
 public:
  SplitFillDrawable(Box window, Color first, uint32_t first_count, Color second,
                    uint32_t second_count)
      : window_(window),
        first_(first),
        first_count_(first_count),
        second_(second),
        second_count_(second_count) {}

  Box extents() const override { return Box(0, 0, 1023, 1023); }

 private:
  void drawTo(const Surface& s) const override {
    DisplayOutput& out = s.out();
    out.setAddress(window_.xMin(), window_.yMin(), window_.xMax(),
                   window_.yMax(), kBlendingSource);
    out.fill(first_, first_count_);
    out.fill(second_, second_count_);
  }

  Box window_;
  Color first_;
  uint32_t first_count_;
  Color second_;
  uint32_t second_count_;
};

Offscreen<Rgb888> MakeCirclePatternSourceBuffer(Color bg, Color big_circle,
                                                Color small_circle) {
  constexpr int kWidth = 50;
  constexpr int kHeight = 50;

  // Draw the source pattern using shape primitives. Using offscreen w/o
  // transparency to force kFillRectangle, but not using Rgb565 to avoid
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
  // - 8x8 (64 px), from a guaranteed white corner region, aligned to 4x4
  //   grid blocks on destination, to create a clear redundant-write
  //   optimization opportunity (white over white).
  // - 1x13 thin strip (1-pixel wide).
  // - 17x5 non-square rectangle.
  DrawClippedRectFromSource(dc, source_image, 0, 0, 7, 7, 8, 8);        // 64
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

TEST(BackgroundFillOptimizer, WriteChunkedUniformStripeOptimization) {
  // Verifies that a uniform write stream split across multiple write() calls
  // still gets collapsed into a background-optimized rectangle.
  constexpr Color kBg = color::White;
  constexpr Color kFill = color::Blue;

  TestScreen screen(16, 16, kBg);
  screen.test().setPalette({kBg, kFill}, kBg);

  Color data[32];
  for (int i = 0; i < 32; ++i) data[i] = kFill;

  screen.setAddress(0, 0, 7, 3, kBlendingSource);
  screen.write(data, 5);
  screen.write(data + 5, 7);
  screen.write(data + 12, 20);

  const uint64_t draw_after_first = screen.test().device().pixelDrawCount();

  screen.setAddress(0, 0, 7, 3, kBlendingSource);
  screen.write(data, 6);
  screen.write(data + 6, 26);

  const uint64_t draw_after_second = screen.test().device().pixelDrawCount();

  EXPECT_CONSISTENT(screen);
  EXPECT_EQ(draw_after_first, 32u);
  EXPECT_EQ(draw_after_second, draw_after_first);
}

TEST(BackgroundFillOptimizer, WritePassthroughThenUniformNextStripe) {
  // Verifies that after entering passthrough on a mixed stripe, write()
  // returns to scan mode at the stripe boundary and can optimize the next
  // uniform stripe.
  constexpr Color kBg = color::White;
  constexpr Color kStripe = color::Green;

  TestScreen screen(16, 16, kBg);
  screen.test().setPalette({kBg, kStripe}, kBg);

  Color data[64];
  for (int i = 0; i < 32; ++i) {
    data[i] = (i % 2 == 0) ? color::Red : color::Blue;
  }
  for (int i = 32; i < 64; ++i) {
    data[i] = kStripe;
  }

  screen.setAddress(0, 0, 7, 7, kBlendingSource);
  screen.write(data, 13);
  screen.write(data + 13, 11);
  screen.write(data + 24, 40);

  const uint64_t draw_after_first = screen.test().device().pixelDrawCount();

  screen.setAddress(0, 0, 7, 7, kBlendingSource);
  screen.write(data, 9);
  screen.write(data + 9, 55);

  const uint64_t draw_after_second = screen.test().device().pixelDrawCount();

  EXPECT_CONSISTENT(screen);
  EXPECT_EQ(draw_after_first, 64u);
  EXPECT_EQ(draw_after_second - draw_after_first, 32u);
}

TEST(BackgroundFillOptimizer, FillColorTransitionFlushesDeferredRun) {
  // Regression test: consecutive fill() calls with different colors while in
  // scan mode must flush the pending deferred run before switching color.
  constexpr Color kBg = color::White;
  constexpr Color kA = color::Blue;
  constexpr Color kB = color::Green;

  TestScreen screen(16, 16, kBg);
  screen.test().setPalette({kBg, kA, kB}, kBg);

  // 8x4 window (32 px) fits one stripe in scan mode.
  // Split into 12 + 20 so both fills occur before stripe-end emission.
  screen.setAddress(0, 0, 7, 3, kBlendingSource);
  screen.fill(kA, 12);
  screen.fill(kB, 20);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillColorTransitionWithOffsetWindow) {
  // Regression test: with an offset window, changing fill color before
  // stripe-end must preserve stream semantics (no rectangularization of a
  // jagged deferred run).
  constexpr Color kBg = color::White;
  constexpr Color kA = color::Blue;
  constexpr Color kB = color::Green;

  TestScreen screen(16, 16, kBg);
  screen.test().setPalette({kBg, kA, kB}, kBg);

  // 8x4 window at y=1. First stripe has only 3 rows (24 px) due 4-row stripe
  // alignment. Split 12 + 20 forces color transition mid-stripe and then
  // continues into the next stripe.
  screen.setAddress(2, 1, 9, 4, kBlendingSource);
  screen.fill(kA, 12);
  screen.fill(kB, 20);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillColorTransitionWithOffsetWindowDrawable) {
  // Regression test that directly exercises BackgroundFillOptimizer::fill()
  // via a custom Drawable.
  constexpr Color kBg = color::White;
  constexpr Color kA = color::Blue;
  constexpr Color kB = color::Green;

  OptimizedDevice<Rgb565> optimized(16, 16, kBg);
  optimized.setPalette({kBg, kA, kB}, kBg);
  FakeOffscreen<Rgb565> reference(16, 16, kBg, Rgb565());

  SplitFillDrawable drawable(Box(2, 1, 9, 4), kA, 12, kB, 20);

  {
    Display display(optimized);
    DrawingContext dc(display);
    dc.draw(drawable);
  }

  {
    Display display(reference);
    DrawingContext dc(display);
    dc.draw(drawable);
  }

  EXPECT_THAT(RasterOf(optimized), MatchesContent(RasterOf(reference)));
}

TEST(BackgroundFillOptimizer, WriteAlignedStripeThenTailAdvancesColorPointer) {
  // Regression test: if tryProcessGridAlignedBlockStripes consumes a full
  // stripe but does not advance the caller's color pointer, the remaining tail
  // pixels are read from the wrong source offset.
  constexpr Color kBg = color::White;
  constexpr Color kStripe = color::Blue;
  constexpr Color kTailA = color::Green;
  constexpr Color kTailB = color::Yellow;

  TestScreen screen(16, 16, kBg);
  screen.test().setPalette({kBg, kStripe, kTailA, kTailB}, kBg);

  Color data[40];
  for (int i = 0; i < 32; ++i) data[i] = kStripe;
  for (int i = 32; i < 40; ++i) data[i] = ((i % 2) == 0) ? kTailA : kTailB;

  // 8x8 destination window is grid-aligned and 4x4-block compatible.
  // First 32 px form one full 8x4 stripe; remaining 8 px are the tail.
  screen.setAddress(0, 0, 7, 7, kBlendingSource);
  screen.write(data, 40);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, WriteSubRectsFromPatternSourceStress) {
  // Stress test for write() path: repeatedly draw random clipped
  // sub-rectangles from an Rgb888 source at random destination offsets,
  // asserting equality with a reference device at each step.
  constexpr Color kBg = color::White;
  constexpr Color kBig = color::Blue;
  constexpr Color kSmall = color::Yellow;
  constexpr int kIterations = 10000;

  TestScreen screen(64, 64, kBg);
  screen.test().setPalette({kBg, kBig}, kBg);

  auto source_image = MakeCirclePatternSourceBuffer(kBg, kBig, kSmall);

  Display display(screen);
  DrawingContext dc(display);

  std::mt19937 rng(123456789u);
  std::uniform_int_distribution<int16_t> width_dist(1, 50);
  std::uniform_int_distribution<int16_t> height_dist(1, 50);

  for (int i = 0; i < kIterations; ++i) {
    const int16_t w = width_dist(rng);
    const int16_t h = height_dist(rng);

    std::uniform_int_distribution<int16_t> sx0_dist(0, 50 - w);
    std::uniform_int_distribution<int16_t> sy0_dist(0, 50 - h);
    std::uniform_int_distribution<int16_t> dx0_dist(0, 64 - w);
    std::uniform_int_distribution<int16_t> dy0_dist(0, 64 - h);

    const int16_t sx0 = sx0_dist(rng);
    const int16_t sy0 = sy0_dist(rng);
    const int16_t sx1 = sx0 + w - 1;
    const int16_t sy1 = sy0 + h - 1;
    const int16_t dx0 = dx0_dist(rng);
    const int16_t dy0 = dy0_dist(rng);

    DrawClippedRectFromSource(dc, source_image, sx0, sy0, sx1, sy1, dx0, dy0);

    SCOPED_TRACE(i);
    ASSERT_THAT(RasterOf(screen.test()),
                MatchesContent(RasterOf(screen.refc())));
  }
}

TEST(BackgroundFillOptimizer, DrawDirectSubRectsFromRgb565PatternSource) {
  // Verifies drawDirectRect path from Raster when source/destination formats
  // match, using sub-rectangle draws similar to WriteSubRectsFromPatternSource.
  constexpr Color kBg = color::White;
  constexpr Color kBig = color::Blue;
  constexpr Color kSmall = color::Yellow;

  TestScreen screen(64, 64, kBg);
  screen.test().setPalette({kBg, kBig}, kBg);

  auto source_image = MakeCirclePatternSourceBufferRgb565(kBg, kBig, kSmall);

  Display display(screen);
  DrawingContext dc(display);

  DrawClippedRectFromSource(dc, source_image, 4, 4, 11, 11, 8, 8);
  DrawClippedRectFromSource(dc, source_image, 46, 18, 46, 30, 41, 26);
  DrawClippedRectFromSource(dc, source_image, 14, 22, 30, 26, 20, 40);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, DrawDirectSubRectsFromRgb565PatternSourceStress) {
  // Stress test for raster direct-draw path: repeatedly draw random clipped
  // sub-rectangles from an Rgb565 source at random destination offsets,
  // asserting equality with a reference device at each step.
  constexpr Color kBg = color::White;
  constexpr Color kBig = color::Blue;
  constexpr Color kSmall = color::Yellow;
  constexpr int kIterations = 10000;

  TestScreen screen(64, 64, kBg);
  screen.test().setPalette({kBg, kBig}, kBg);

  auto source_image = MakeCirclePatternSourceBufferRgb565(kBg, kBig, kSmall);

  Display display(screen);
  DrawingContext dc(display);

  std::mt19937 rng(123456789u);
  std::uniform_int_distribution<int16_t> width_dist(1, 50);
  std::uniform_int_distribution<int16_t> height_dist(1, 50);

  for (int i = 0; i < kIterations; ++i) {
    const int16_t w = width_dist(rng);
    const int16_t h = height_dist(rng);

    std::uniform_int_distribution<int16_t> sx0_dist(0, 50 - w);
    std::uniform_int_distribution<int16_t> sy0_dist(0, 50 - h);
    std::uniform_int_distribution<int16_t> dx0_dist(0, 64 - w);
    std::uniform_int_distribution<int16_t> dy0_dist(0, 64 - h);

    const int16_t sx0 = sx0_dist(rng);
    const int16_t sy0 = sy0_dist(rng);
    const int16_t sx1 = sx0 + w - 1;
    const int16_t sy1 = sy0 + h - 1;
    const int16_t dx0 = dx0_dist(rng);
    const int16_t dy0 = dy0_dist(rng);

    DrawClippedRectFromSource(dc, source_image, sx0, sy0, sx1, sy1, dx0, dy0);

    SCOPED_TRACE(i);
    ASSERT_THAT(RasterOf(screen.test()),
                MatchesContent(RasterOf(screen.refc())));
  }
}

}  // namespace roo_display

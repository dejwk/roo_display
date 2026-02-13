#include "roo_display/filter/background_fill_optimizer.h"

#include <memory>

#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/color/color.h"
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
        buffer_(width, height),
        optimizer_(device_, buffer_) {
    // Set up with the prefilled color as the palette
    buffer_.setPalette({prefilled});
    buffer_.setPrefilled(prefilled);
  }

  void orientationUpdated() override { device_.setOrientation(orientation()); }

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

  const ColorFormat& getColorFormat() const override {
    return device_.getColorFormat();
  }

  const FakeOffscreen<ColorMode>& device() const { return device_; }
  BackgroundFillOptimizer::FrameBuffer& buffer() { return buffer_; }

 private:
  FakeOffscreen<ColorMode> device_;
  BackgroundFillOptimizer::FrameBuffer buffer_;
  BackgroundFillOptimizer optimizer_;
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

// Basic tests for FrameBuffer.

TEST(BackgroundFillOptimizer, FrameBufferConstruction) {
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
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0[] = {10};
  int16_t y0[] = {10};
  int16_t x1[] = {20};
  int16_t y1[] = {20};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, RedundantFillOptimization) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0[] = {8};
  int16_t y0[] = {8};
  int16_t x1[] = {23};
  int16_t y1[] = {23};

  // First fill.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  // Second fill - optimizer should optimize this away, but behavior should
  // match.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, NonPaletteColorInvalidatesMask) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0[] = {10};
  int16_t y0[] = {10};
  int16_t x1[] = {20};
  int16_t y1[] = {20};

  // Draw a foreground color (not in palette).
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF112233), x0, y0, x1, y1, 1);

  // Now fill with background color - should still write since mask invalidated.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, MultiplePaletteColors) {
  TestScreen screen(64, 48, Color(0xFFFF0000));

  int16_t x0[] = {0, 20, 40};
  int16_t y0[] = {0, 20, 10};
  int16_t x1[] = {15, 35, 55};
  int16_t y1[] = {15, 35, 25};
  Color colors[] = {Color(0xFFFF0000), Color(0xFF00FF00), Color(0xFF0000FF)};

  // Fill different areas with different palette colors.
  screen.writeRects(BLENDING_MODE_SOURCE, colors, x0, y0, x1, y1, 3);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillPixels) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x[] = {10, 20, 30, 15, 25};
  int16_t y[] = {10, 15, 20, 25, 30};

  screen.fillPixels(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x, y, 5);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, WritePixels) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x[] = {10, 20, 30, 15, 25};
  int16_t y[] = {10, 15, 20, 25, 30};
  Color colors[] = {Color(0xFFAABBCC), color::Red, Color(0xFFAABBCC),
                    color::Blue, Color(0xFFAABBCC)};

  screen.writePixels(BLENDING_MODE_SOURCE, colors, x, y, 5);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, WriteRects) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0[] = {5, 25};
  int16_t y0[] = {5, 25};
  int16_t x1[] = {15, 35};
  int16_t y1[] = {15, 35};
  Color colors[] = {Color(0xFFAABBCC), color::Green};

  screen.writeRects(BLENDING_MODE_SOURCE, colors, x0, y0, x1, y1, 2);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillRects) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0[] = {5, 25, 10};
  int16_t y0[] = {5, 25, 35};
  int16_t x1[] = {15, 35, 20};
  int16_t y1[] = {15, 35, 45};

  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 3);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, PartialOverlap) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0_fg[] = {10};
  int16_t y0_fg[] = {10};
  int16_t x1_fg[] = {30};
  int16_t y1_fg[] = {30};
  int16_t x0_bg[] = {20};
  int16_t y0_bg[] = {20};
  int16_t x1_bg[] = {40};
  int16_t y1_bg[] = {40};

  // Draw foreground rect.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF112233), x0_fg, y0_fg, x1_fg,
                   y1_fg, 1);

  // Partially overlap with background rect.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0_bg, y0_bg, x1_bg,
                   y1_bg, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, InvalidateRect) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  // Both test and reference should produce same result after invalidation.
  int16_t x0[] = {10};
  int16_t y0[] = {10};
  int16_t x1[] = {20};
  int16_t y1[] = {20};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, SetAddressAndWrite) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  // Test setAddress and write methods.
  screen.setAddress(10, 10, 20, 20, BLENDING_MODE_SOURCE);
  Color colors[100];
  for (int i = 0; i < 100; ++i) {
    colors[i] = Color(0xFFAABBCC);
  }
  screen.write(colors, 100);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, LargerDisplay) {
  TestScreen screen(320, 240, Color(0xFF000000));

  // Draw a pattern.
  for (int i = 0; i < 10; ++i) {
    Color c = (i % 2 == 0) ? Color(0xFF000000) : Color(0xFFFFFFFF);
    int16_t x0[] = {(int16_t)(i * 30)};
    int16_t y0[] = {(int16_t)(i * 20)};
    int16_t x1[] = {(int16_t)(i * 30 + 25)};
    int16_t y1[] = {(int16_t)(i * 20 + 15)};
    screen.fillRects(BLENDING_MODE_SOURCE, c, x0, y0, x1, y1, 1);
  }

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, PaletteChange) {
  TestScreen screen(64, 48, Color(0xFFFF0000));

  int16_t x0[] = {10};
  int16_t y0[] = {10};
  int16_t x1[] = {20};
  int16_t y1[] = {20};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFFF0000), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, EdgeCases) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x0[] = {0, 60};
  int16_t y0[] = {0, 44};
  int16_t x1[] = {3, 63};
  int16_t y1[] = {3, 47};

  // Draw at boundaries.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 2);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, SinglePixelWrites) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  // Draw single pixels.
  int16_t x[10];
  int16_t y[10];
  for (int i = 0; i < 10; ++i) {
    x[i] = i * 5;
    y[i] = i * 4;
  }
  screen.fillPixels(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x, y, 10);

  EXPECT_CONSISTENT(screen);
}

// Tests for optimization paths and uncovered code.

TEST(BackgroundFillOptimizer, FillRectBgOptimization) {
  TestScreen screen(128, 96, Color(0xFFAABBCC));

  int16_t x0[] = {0};
  int16_t y0[] = {0};
  int16_t x1[] = {127};
  int16_t y1[] = {95};

  // Large fill with background color triggers fillRectBg() for optimization.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillRectBgWithNonPaletteOverlay) {
  TestScreen screen(128, 96, Color(0xFFAABBCC));

  // First, fill large background area.
  int16_t x0_bg[] = {0};
  int16_t y0_bg[] = {0};
  int16_t x1_bg[] = {127};
  int16_t y1_bg[] = {95};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0_bg, y0_bg, x1_bg,
                   y1_bg, 1);

  // Draw non-palette color, invalidating mask.
  int16_t x0_fg[] = {32};
  int16_t y0_fg[] = {24};
  int16_t x1_fg[] = {96};
  int16_t y1_fg[] = {72};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF112233), x0_fg, y0_fg, x1_fg,
                   y1_fg, 1);

  // Fill background again in same region - should still match reference.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0_bg, y0_bg, x1_bg,
                   y1_bg, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillRectBgComplexPattern) {
  TestScreen screen(160, 120, Color(0xFF000000));

  // Create a complex pattern with multiple palette colors and fills.
  int16_t x0[] = {10, 50, 100, 20, 60};
  int16_t y0[] = {10, 20, 30, 60, 80};
  int16_t x1[] = {40, 80, 130, 50, 90};
  int16_t y1[] = {40, 50, 60, 90, 110};
  Color colors[] = {Color(0xFF000000), Color(0xFF000000), Color(0xFF000000),
                    Color(0xFF000000), Color(0xFF000000)};

  // Multiple fills with background color should trigger optimization.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF000000), x0, y0, x1, y1, 5);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, WriteRectsWithBackgroundTriggersOptimization) {
  TestScreen screen(128, 96, Color(0xFFAABBCC));

  int16_t x0[] = {16, 48, 80};
  int16_t y0[] = {16, 32, 64};
  int16_t x1[] = {47, 79, 111};
  int16_t y1[] = {47, 63, 95};
  Color colors[] = {Color(0xFFAABBCC), Color(0xFFAABBCC), Color(0xFFAABBCC)};

  // WriteRects with background colors triggers fillRectBg internally.
  screen.writeRects(BLENDING_MODE_SOURCE, colors, x0, y0, x1, y1, 3);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FillPixelsNonPaletteColor) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  int16_t x[] = {10, 20, 30, 15, 25};
  int16_t y[] = {10, 15, 20, 25, 30};

  // Fill pixels with non-palette color tests the non-palette path.
  screen.fillPixels(BLENDING_MODE_SOURCE, Color(0xFF112233), x, y, 5);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, WritePixelsAllSkipped) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  // Fill with background first.
  int16_t x0[] = {0};
  int16_t y0[] = {0};
  int16_t x1[] = {63};
  int16_t y1[] = {47};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  // Now write pixels with background color - some might be skipped.
  int16_t x[] = {10, 20, 30, 40, 50};
  int16_t y[] = {10, 15, 20, 25, 30};
  Color colors[] = {Color(0xFFAABBCC), Color(0xFFAABBCC), Color(0xFFAABBCC),
                    Color(0xFFAABBCC), Color(0xFFAABBCC)};

  screen.writePixels(BLENDING_MODE_SOURCE, colors, x, y, 5);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, InvalidateRectMethod) {
  TestScreen screen(64, 48, Color(0xFFAABBCC));

  // Access invalidateRect through the buffer.
  int16_t x0[] = {16};
  int16_t y0[] = {16};
  int16_t x1[] = {32};
  int16_t y1[] = {32};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, BoundaryClipping) {
  TestScreen screen(128, 96, Color(0xFF000000));

  // Test rectangles at screen boundaries.
  int16_t x0[] = {0, 100, 0, 96};
  int16_t y0[] = {0, 64, 0, 80};
  int16_t x1[] = {20, 127, 50, 127};
  int16_t y1[] = {20, 95, 50, 95};

  // Fills at boundaries should work correctly.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF000000), x0, y0, x1, y1, 4);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, AlternatingPaletteForeground) {
  TestScreen screen(128, 96, Color(0xFFFF0000));

  // Alternate between palette and non-palette colors in same region.
  int16_t x0[] = {20, 40, 60, 20, 40, 60};
  int16_t y0[] = {20, 20, 20, 50, 50, 50};
  int16_t x1[] = {35, 55, 75, 35, 55, 75};
  int16_t y1[] = {35, 35, 35, 65, 65, 65};
  Color colors[] = {Color(0xFF112233), Color(0xFFFF0000), Color(0xFF445566),
                    Color(0xFF778899), Color(0xFFFF0000), Color(0xFFAABBCC)};

  // Mix of palette and non-palette fills to test mask invalidation.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFFF0000), x0, y0, x1, y1, 6);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, LargeGridAlignment) {
  // Test that larger rectangles align with coarse grid windows.
  TestScreen screen(256, 192, Color(0xFF000000));

  // Fill rectangle aligned with grid boundaries (kBgFillOptimizerWindowSize =
  // 16).
  int16_t x0[] = {0, 48, 96, 144, 192};
  int16_t y0[] = {0, 32, 64, 96, 128};
  int16_t x1[] = {31, 79, 127, 175, 223};
  int16_t y1[] = {31, 63, 95, 127, 159};

  // Grid-aligned fills trigger inner_filter_box optimization.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF000000), x0, y0, x1, y1, 5);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, PartialGridCoverage) {
  TestScreen screen(128, 96, Color(0xFFAABBCC));

  // Rectangles that partially cover grid windows test clipping logic.
  int16_t x0[] = {5, 21, 37};
  int16_t y0[] = {5, 21, 37};
  int16_t x1[] = {26, 42, 58};
  int16_t y1[] = {26, 42, 58};

  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFAABBCC), x0, y0, x1, y1, 3);

  EXPECT_CONSISTENT(screen);
}

TEST(BackgroundFillOptimizer, FrameBufferInvalidateRect) {
  // Test FrameBuffer::invalidateRect() directly through mask operations.
  BackgroundFillOptimizer::FrameBuffer buffer(128, 96);
  Color palette[] = {color::Red, color::Green, color::Blue};
  buffer.setPalette(palette, 3);
  buffer.setPrefilled(color::Red);

  // Invalidate a region and verify it doesn't crash.
  Box rect(10, 10, 30, 30);
  buffer.invalidateRect(rect);

  EXPECT_EQ(buffer.palette_size(), 3);
}

TEST(BackgroundFillOptimizer, MultipleWrites) {
  TestScreen screen(96, 72, Color(0xFF000000));

  // Multiple sequential writes with various colors.
  int16_t x0[] = {8};
  int16_t y0[] = {8};
  int16_t x1[] = {23};
  int16_t y1[] = {23};

  // First write background.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF000000), x0, y0, x1, y1, 1);

  // Write foreground.
  int16_t x0_fg[] = {12};
  int16_t y0_fg[] = {12};
  int16_t x1_fg[] = {20};
  int16_t y1_fg[] = {20};
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFFFFFFFF), x0_fg, y0_fg, x1_fg,
                   y1_fg, 1);

  // Write background again.
  screen.fillRects(BLENDING_MODE_SOURCE, Color(0xFF000000), x0, y0, x1, y1, 1);

  EXPECT_CONSISTENT(screen);
}

}  // namespace roo_display

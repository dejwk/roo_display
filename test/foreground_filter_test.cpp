
#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/filter/foreground.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"
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
  TestDrawDirectRect<TestDeviceSimple, RefDeviceSimple>(BlendingMode::kSource,
                                                        Orientation());
}

TEST(Background, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
}

TEST(Foreground, InheritedFillRefillsSourceBufferBetweenWrites) {
  FakeOffscreen<Argb8888> actual(130, 1, color::White);
  FakeOffscreen<Argb8888> expected(130, 1, color::White);
  FilledRect foreground(Box(0, 0, 1, 0), color::Red);
  ForegroundFilter filter(actual, &foreground);

  filter.setAddress(0, 0, 129, 0, BlendingMode::kSource);
  // Exercise DisplayOutput's default fill implementation with a write()
  // implementation that is permitted to modify the supplied buffer.
  filter.DisplayOutput::fill(color::Transparent, 130);

  expected.fillRect(BlendingMode::kSource, Box(0, 0, 1, 0), color::Red);
  expected.fillRect(BlendingMode::kSource, Box(2, 0, 129, 0),
                    color::Transparent);
  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

TEST(Foreground, OptimizedFillMatchesWritableBufferFallback) {
  FakeOffscreen<Argb4444> optimized(96, 32, color::White);
  FakeOffscreen<Argb4444> fallback(96, 32, color::White);
  SmoothShape foreground =
      SmoothFilledRoundRect(40, 8, 63, 23, 8, Color(0xffba1a1a));
  ForegroundFilter optimized_filter(optimized, &foreground);
  ForegroundFilter fallback_filter(fallback, &foreground);

  optimized_filter.setAddress(0, 0, 95, 31, BlendingMode::kSource);
  optimized_filter.fill(Color(0xff444455), 96 * 32);
  fallback_filter.setAddress(0, 0, 95, 31, BlendingMode::kSource);
  fallback_filter.DisplayOutput::fill(Color(0xff444455), 96 * 32);

  EXPECT_THAT(RasterOf(optimized), MatchesContent(RasterOf(fallback)));
}

namespace {

class TrackedUniformRaster : public FilledRect {
 public:
  TrackedUniformRaster(Box bounds, Color color) : FilledRect(bounds, color) {}

  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override {
    class Stream : public PixelStream {
     public:
      Stream(std::unique_ptr<PixelStream> delegate, uint32_t& sampled,
             uint32_t& skipped)
          : delegate_(std::move(delegate)),
            sampled_(sampled),
            skipped_(skipped) {}

      void read(Color* result, uint16_t count, uint32_t& run) override {
        sampled_ += count;
        delegate_->read(result, count, run);
      }

      void skip(uint32_t count) override {
        skipped_ += count;
        delegate_->skip(count);
      }

     private:
      std::unique_ptr<PixelStream> delegate_;
      uint32_t& sampled_;
      uint32_t& skipped_;
    };
    return std::make_unique<Stream>(FilledRect::createStream(bounds), sampled,
                                    skipped);
  }

  mutable uint32_t sampled = 0;
  mutable uint32_t skipped = 0;
};

template <BlendingMode mode>
void CheckUniformRasterWrites() {
  const Box bounds(0, 0, 159, 9);
  const Color colors[] = {color::Transparent, color::Background,
                          Color(0x00123456), Color(0x80987654), color::Blue};
  for (Color uniform : {color::Transparent, color::Background,
                        Color(0x00123456), Color(0x80654321), color::White}) {
    for (Color background :
         {color::Transparent, Color(0x80345678), color::White}) {
      TrackedUniformRaster raster(bounds, uniform);
      FakeOffscreen<Argb8888> output(160, 10);
      BlendingFilter<BlendOp<mode>> filter(output, &raster, background);
      std::vector<Color> pixels(bounds.area());
      for (int i = 0; i < bounds.area(); ++i) pixels[i] = colors[i % 5];
      filter.setAddress(0, 0, 159, 9, BlendingMode::kSource);
      filter.write(pixels.data(), pixels.size());
      for (int i = 0; i < bounds.area(); ++i) {
        Color expected = ApplyBlending(mode, uniform, colors[i % 5]);
        if (background != color::Transparent)
          expected = AlphaBlend(background, expected);
        EXPECT_EQ(output.buffer()[i], expected);
      }
      EXPECT_LE(raster.sampled, kPixelWritingBufferSize);
      EXPECT_EQ(raster.sampled + raster.skipped,
                static_cast<uint32_t>(bounds.area()));
    }
  }
}

}  // namespace

// Verifies nonuniform writes reuse a uniform mask or foreground and preserve
// special colors and backgrounds without allocating scratch for the full write.
TEST(Foreground, UniformRasterWritesSkipRedundantSamples) {
  CheckUniformRasterWrites<BlendingMode::kDestinationOver>();
  CheckUniformRasterWrites<BlendingMode::kSourceIn>();
}

// Verifies an unlimited delegate run ends at the clipped row and cannot paint
// into transparent gaps, through both fill() and nonuniform write() consumers.
TEST(Foreground, UniformRunDoesNotCrossWindowGaps) {
  const Box bounds(3, 0, 66, 9);
  for (bool fill : {false, true}) {
    TrackedUniformRaster raster(bounds, Color(0x80654321));
    FakeOffscreen<Argb8888> output(80, 10);
    ForegroundFilter filter(output, &raster);
    filter.setAddress(0, 0, 79, 9, BlendingMode::kSource);
    std::vector<Color> pixels(800, color::Blue);
    filter.write(pixels.data(), 3);
    if (fill) {
      filter.fill(color::Blue, 797);
    } else {
      filter.write(pixels.data() + 3, 797);
    }
    for (int i = 0; i < 800; ++i) {
      Color expected = bounds.contains(i % 80, i / 80)
                           ? AlphaBlend(color::Blue, Color(0x80654321))
                           : color::Blue;
      EXPECT_EQ(output.buffer()[i], expected) << i;
    }
  }
}

// Verifies a read spanning the leading gap, entire mask, and trailing gap does
// not advertise the interior color's run as a promise about the leading pixels.
TEST(Foreground, SmallRasterBetweenGapsDoesNotAdvertiseInteriorRun) {
  FilledRect foreground(Box(3, 0, 10, 0), Color(0x80654321));
  internal::WindowedPixelStream stream(&foreground);
  stream.reset(Box(0, 0, 79, 0));
  Color pixels[80];
  uint32_t run = 999;
  stream.read(pixels, 80, run);
  EXPECT_LE(run, 3u);
  for (int i = 0; i < 80; ++i) {
    EXPECT_EQ(pixels[i],
              i >= 3 && i <= 10 ? Color(0x80654321) : color::Transparent);
  }
  for (bool fill : {false, true}) {
    FakeOffscreen<Argb8888> output(80, 1);
    ForegroundFilter filter(output, &foreground);
    filter.setAddress(0, 0, 79, 0, BlendingMode::kSource);
    if (fill) {
      filter.fill(color::Blue, 80);
    } else {
      FillColor(pixels, 80, color::Blue);
      filter.write(pixels, 80);
    }
    for (int i = 0; i < 80; ++i) {
      EXPECT_EQ(output.buffer()[i],
                i >= 3 && i <= 10 ? AlphaBlend(color::Blue, Color(0x80654321))
                                  : color::Blue);
    }
  }
}

}  // namespace roo_display

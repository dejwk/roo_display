
#include "roo_display/filter/clip_mask.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

class SimpleRoundMask {
 public:
  SimpleRoundMask(Box extents) {}

  template <typename ColorMode>
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    static const char mask[] =
        "                "
        "   *******      "
        "  *********     "
        " ***********    "
        "  *********     "
        "   *******      "
        "                ";
    if (Box(1, 2, 16, 8).contains(x, y) && mask[x - 1 + (y - 2) * 16] == '*')
      return;
    offscreen->writePixel(mode, x, y, color);
  }

  static ClipMaskFilter* Create(DisplayOutput& output, Box extents) {
    static const uint8_t clip_mask_data[] = {
        0x00, 0x00, 0x1F, 0xC0, 0x3F, 0xE0, 0x7F,
        0xF0, 0x3F, 0xE0, 0x1F, 0xC0, 0x00, 0x00,
    };
    static ClipMask mask(clip_mask_data, Box(1, 2, 16, 8));
    return new ClipMaskFilter(output, &mask);
  }
};

class LargeMask {
 public:
  LargeMask(Box extents) {}

  template <typename ColorMode>
  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color,
                  FakeOffscreen<ColorMode>* offscreen) {
    static const char mask[] =
        "                                "
        "   ***********************      "
        "  *************************     "
        "  *************************     "
        " ***************************    "
        " ***************************    "
        " ***************************    "
        "  *************************     "
        "  *************************     "
        "   ***********************      "
        "   ***********************      "
        "   ***********************      "
        "  *************************     "
        "  *************************     "
        " ***************************    "
        " ***************************    "
        " ***************************    "
        "  *************************     "
        "  *************************     "
        "   ***********************      "
        "                                ";
    if (Box(1, 2, 32, 22).contains(x, y) && mask[x - 1 + (y - 2) * 32] == '*')
      return;
    offscreen->writePixel(mode, x, y, color);
  }

  static ClipMaskFilter* Create(DisplayOutput& output, Box extents) {
    static const uint8_t clip_mask_data[] = {
        0b00000000, 0b00000000, 0b00000000, 0b00000000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b01111111, 0b11111111, 0b11111111, 0b11110000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00111111, 0b11111111, 0b11111111, 0b11100000,  // NOFORMAT
        0b00011111, 0b11111111, 0b11111111, 0b11000000,  // NOFORMAT
        0b00000000, 0b00000000, 0b00000000, 0b00000000,  // NOFORMAT
    };
    static ClipMask mask(clip_mask_data, Box(1, 2, 32, 22));
    return new ClipMaskFilter(output, &mask);
  }
};

typedef FakeFilteringOffscreen<Grayscale4, SimpleRoundMask> RefDeviceSimple;
typedef FilteredOutput<Grayscale4, SimpleRoundMask> TestDeviceSimple;

TEST(ClipMask, SimpleTests) {
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

TEST(ClipMask, StressTests) {
  TestWritePixelsStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
  TestWriteRectWindowStress<TestDeviceSimple, RefDeviceSimple>(
      BlendingMode::kSource, Orientation());
}

typedef FakeFilteringOffscreen<Grayscale4, LargeMask> RefDeviceLarge;
typedef FilteredOutput<Grayscale4, LargeMask> TestDeviceLarge;

TEST(ClipMask, SimpleLargeTests) {
  TestFillRects<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                 Orientation());
  TestFillHLines<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                  Orientation());
  TestFillVLines<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                  Orientation());
  TestFillDegeneratePixels<TestDeviceLarge, RefDeviceLarge>(
      BlendingMode::kSource, Orientation());
  TestFillPixels<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                  Orientation());

  TestWriteRects<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                  Orientation());
  TestWriteHLines<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                   Orientation());
  TestWriteVLines<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                   Orientation());
  TestWriteDegeneratePixels<TestDeviceLarge, RefDeviceLarge>(
      BlendingMode::kSource, Orientation());
  TestWritePixels<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                   Orientation());
  TestWritePixelsSnake<TestDeviceLarge, RefDeviceLarge>(BlendingMode::kSource,
                                                        Orientation());
  TestWriteRectWindowSimple<TestDeviceLarge, RefDeviceLarge>(
      BlendingMode::kSource, Orientation());
}

TEST(ClipMask, ClipMaskWrite) {
  FakeOffscreen<Rgb565> test_screen(16, 7);
  Display display(test_screen);
  const uint8_t clip_mask_data[] = {
      0xFF, 0xFF, 0xE0, 0x3F, 0xC0, 0x1F, 0x80,
      0x0F, 0xC0, 0x1F, 0xE0, 0x3F, 0xFF, 0xFF,
  };
  ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
  {
    DrawingContext dc(display);
    dc.setClipMask(&mask);
    dc.draw(MakeTestStreamable(WhiteOnBlack(), Box(0, 0, 13, 5),
                               "**************"
                               "          ****"
                               "**************"
                               "**************"
                               "***           "
                               "***           "
                               "**************"),
            1, 0);
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 16, 7,
                                          "                "
                                          "                "
                                          "  *********     "
                                          " ***********    "
                                          "  **            "
                                          "   *            "
                                          "                "));
}

TEST(ClipMask, ClipMaskStreamableSemiTransparent) {
  FakeOffscreen<Rgb565> test_screen(16, 7);
  Display display(test_screen);
  const uint8_t clip_mask_data[] = {
      0xFF, 0xFF, 0xE0, 0x3F, 0xC0, 0x1F, 0x80,
      0x0F, 0xC0, 0x1F, 0xE0, 0x3F, 0xFF, 0xFF,
  };
  ClipMask mask(clip_mask_data, Box(0, 0, 15, 6));
  {
    DrawingContext dc(display);
    dc.setClipMask(&mask);
    dc.draw(MakeTestStreamable(Alpha4(color::White), Box(0, 0, 13, 5),
                               "**************"
                               "          ****"
                               "**************"
                               "**************"
                               "***           "
                               "***           "
                               "**************"),
            1, 0);
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 16, 7,
                                          "                "
                                          "                "
                                          "  *********     "
                                          " ***********    "
                                          "  **            "
                                          "   *            "
                                          "                "));
}

namespace {
class MaskCountingOutput : public FakeOffscreen<Argb8888> {
 public:
  using FakeOffscreen::FakeOffscreen;

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    ++addresses;
    FakeOffscreen::setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* colors, uint32_t count) override {
    ++writes;
    FakeOffscreen::write(colors, count);
  }

  void writePixels(BlendingMode mode, Color* colors, int16_t* x, int16_t* y,
                   uint16_t count) override {
    ++pixel_batches;
    FakeOffscreen::writePixels(mode, colors, x, y, count);
  }

  int addresses = 0;
  int writes = 0;
  int pixel_batches = 0;
};
}  // namespace

// Verifies uninterrupted visible data keeps one address window across calls.
TEST(ClipMask, VisibleWindowForwardsBulkWrites) {
  MaskCountingOutput output(19, 7);
  roo::byte bits[21] = {};
  ClipMask mask(bits, Box(0, 0, 18, 6));
  ClipMaskFilter filter(output, &mask);
  std::vector<Color> colors(133, color::Blue);
  filter.setAddress(0, 0, 18, 6, BlendingMode::kSource);
  filter.write(colors.data(), 13);
  filter.write(colors.data() + 13, 120);
  EXPECT_EQ(output.addresses, 1);
  EXPECT_EQ(output.writes, 2);
  EXPECT_EQ(output.pixel_batches, 0);
  for (int i = 0; i < 133; ++i) EXPECT_EQ(output.buffer()[i], color::Blue);
}

// Verifies byte runs, inversion, padding, and outside-mask pixels against the
// per-pixel oracle, with mixed write/fill calls ending inside rows and bytes.
TEST(ClipMask, SpanWritesMatchPixelOracle) {
  for (bool inverted : {false, true}) {
    for (int pattern = 0; pattern < 256; ++pattern) {
      roo::byte bits[8];
      for (int i = 0; i < 8; ++i) bits[i] = roo::byte((pattern + 29 * i) & 255);
      ClipMask mask(bits, Box(3, 2, 12, 5), inverted);
      MaskCountingOutput output(19, 7);
      FakeOffscreen<Argb8888> expected(19, 7);
      ClipMaskFilter filter(output, &mask);
      filter.setAddress(0, 0, 18, 6, BlendingMode::kSourceOver);
      for (int offset = 0; offset < 133;) {
        int count = std::min(1 + offset % 23, 133 - offset);
        std::vector<Color> colors(count);
        bool fill = offset % 3 == 0;
        for (int i = 0; i < count; ++i) {
          colors[i] = fill ? color::Red : Color(128, offset + i, 40, 70);
          int x = (offset + i) % 19;
          int y = (offset + i) / 19;
          if (!mask.isMasked(x, y)) {
            expected.writePixel(BlendingMode::kSourceOver, x, y, colors[i]);
          }
        }
        if (fill)
          filter.fill(color::Red, count);
        else
          filter.write(colors.data(), count);
        offset += count;
      }
      EXPECT_THAT(RasterOf(output), MatchesContent(RasterOf(expected)))
          << pattern << "/" << inverted;
      EXPECT_EQ(output.pixel_batches, 0);
    }
  }
}

// Verifies future mask changes and intervening coordinate writes invalidate
// visibility/address assumptions while the original stream cursor is retained.
TEST(ClipMask, MutableMaskAndInterleavedOutput) {
  MaskCountingOutput output(8, 2);
  roo::byte bits[2] = {};
  ClipMask mask(bits, Box(0, 0, 7, 1));
  ClipMaskFilter filter(output, &mask);
  filter.setAddress(0, 0, 7, 1, BlendingMode::kSource);
  filter.fill(color::Blue, 3);
  bits[0] = roo::byte{0x18};  // Hide the next two pixels after the first call.
  filter.fill(color::Red, 5);
  int16_t x = 0;
  int16_t y = 0;
  filter.fillPixels(BlendingMode::kSource, color::Green, &x, &y, 1);
  filter.fill(color::White, 8);
  EXPECT_EQ(output.buffer()[0], color::Green);
  EXPECT_EQ(output.buffer()[3], color::Transparent);
  EXPECT_EQ(output.buffer()[4], color::Transparent);
  EXPECT_EQ(output.buffer()[5], color::Red);
  for (int i = 8; i < 16; ++i) EXPECT_EQ(output.buffer()[i], color::White);
  EXPECT_EQ(output.addresses, 3);
}

}  // namespace roo_display

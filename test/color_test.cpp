#include "roo_display/core/color.h"
#include "gtest/gtest.h"

namespace roo_display {

std::ostream& operator<<(std::ostream& os, const Color& color) {
  return os << "argb:" << std::hex << color.asArgb();
}

template <typename ColorMode>
void checkSameAfterConversion(Color color, ColorMode mode = ColorMode()) {
  EXPECT_EQ(color, mode.toArgbColor(mode.fromArgbColor(color)));
}

template <typename ColorMode>
void basicColorTest(ColorMode mode = ColorMode()) {
  checkSameAfterConversion<ColorMode>(Color(0xFFFFFFFF), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFF000000), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFFFF0000), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFF00FF00), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFF0000FF), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFF00FFFF), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFFFF00FF), mode);
  checkSameAfterConversion<ColorMode>(Color(0xFFFFFF00), mode);
}

TEST(Color, BasicArgb8888) { basicColorTest<Argb8888>(); }

TEST(Color, BasicArgb6666) { basicColorTest<Argb6666>(); }

TEST(Color, BasicArgb4444) { basicColorTest<Argb4444>(); }

TEST(Color, BasicRgb565) { basicColorTest<Rgb565>(); }

template <typename ColorMode>
void testConsistent(ColorMode mode = ColorMode()) {
  ColorStorageType<ColorMode> raw = (1 << ColorMode::bits_per_pixel) - 1;
  while (true) {
    Color argb = mode.toArgbColor(raw);
    ColorStorageType<ColorMode> converted = mode.fromArgbColor(argb);
    ASSERT_EQ(raw, converted)
        << std::hex << raw << ", " << argb << ", " << converted;
    if (raw == 0) break;
    raw--;
  }
}

TEST(Color, ConsistentRgb565) { testConsistent(Rgb565()); }

TEST(Color, ConsistentArgb4444) { testConsistent(Argb4444()); }

TEST(Color, Truncate565Adafruit) {
  Rgb565 mode;
  EXPECT_EQ(0x0000, mode.fromArgbColor(color::Black));
  EXPECT_EQ(0x000F, mode.fromArgbColor(color::Navy));
  EXPECT_EQ(0x7800, mode.fromArgbColor(color::Maroon));
  EXPECT_EQ(0x780F, mode.fromArgbColor(color::Purple));
  EXPECT_EQ(0x7BE0, mode.fromArgbColor(color::Olive));
  EXPECT_EQ(0x001F, mode.fromArgbColor(color::Blue));
  EXPECT_EQ(0x07FF, mode.fromArgbColor(color::Cyan));
  EXPECT_EQ(0xF800, mode.fromArgbColor(color::Red));
  EXPECT_EQ(0xF81F, mode.fromArgbColor(color::Magenta));
  EXPECT_EQ(0xFFE0, mode.fromArgbColor(color::Yellow));
  EXPECT_EQ(0xFFFF, mode.fromArgbColor(color::White));
  EXPECT_EQ(0xFD20, mode.fromArgbColor(color::Orange));
  EXPECT_EQ(0xAFE5, mode.fromArgbColor(color::GreenYellow));
}

// Colors that are known to be different than similarly named Adafruit colors.
TEST(Color, Truncate565AdafruitIncompatible) {
  Rgb565 mode;
  // Adafruit's DARK_GREEN is #008000 = just a regular Green.
  EXPECT_EQ(0x0320, mode.fromArgbColor(color::DarkGreen));
  // Adafruit's DARK_CYAN is #008080, slightly darker than #008B8B.
  EXPECT_EQ(0x0451, mode.fromArgbColor(color::DarkCyan));
  // Adafruit's LIGHTGREY is #C6C6C6, slightly darker than #D3D3D3.
  EXPECT_EQ(0xD69A, mode.fromArgbColor(color::LightGray));
  // Adafruit's DARKGREY is #808080, like regular Gray.
  EXPECT_EQ(0xA554, mode.fromArgbColor(color::DarkGray));
  // Adafruit's GREEN is #00FF00, like Lime.
  EXPECT_EQ(0x03E0, mode.fromArgbColor(color::Green));
  // Adafruit's PINK is #FF82C6, closer to HotPink, vs Pink = #FFC0CB.
  EXPECT_EQ(0xFDF9, mode.fromArgbColor(color::Pink));
}

TEST(Color, Grayscale4) { testConsistent(Grayscale4()); }

TEST(Color, Alpha8) {
  Alpha8 mode(Color(0xCCF0F0F0));
  EXPECT_EQ(Color(0xE3F0F0F0), mode.toArgbColor(0xE3));
  EXPECT_EQ(0xD5, mode.fromArgbColor(Color(0xD51343f3)));
}

TEST(Color, Alpha4) {
  Alpha4 mode(Color(0xCCF0F0F0));
  EXPECT_EQ(Color(0xEEF0F0F0), mode.toArgbColor(0x0E));
  EXPECT_EQ(0x0D, mode.fromArgbColor(Color(0xDC1343f3)));
}

TEST(Color, MonochromeToArgbColor) {
  Color fg(0xDAA3E0F5);
  Color bg(0xF1311F3A);
  Monochrome mode(fg, bg);
  EXPECT_EQ(fg, mode.toArgbColor(1));
  EXPECT_EQ(bg, mode.toArgbColor(0));
}

TEST(Color, MonochromeCopy) {
  Color fg(0xDAA3E0F5);
  Color bg(0xF1311F3A);
  Monochrome mode(fg, bg);
  EXPECT_EQ(fg, mode.fg());
  EXPECT_EQ(bg, mode.bg());

  Monochrome other_mode = mode;
  EXPECT_EQ(fg, other_mode.fg());
  EXPECT_EQ(bg, other_mode.bg());
}

TEST(Color, MonochromeFromArgbColor) {
  Color fg(0xDAA3E0F5);
  Color bg(0xF1311F3A);
  Color other(0xC1344F18);
  Monochrome mode(fg, bg);
  EXPECT_EQ(1, mode.fromArgbColor(fg));
  EXPECT_EQ(0, mode.fromArgbColor(bg));
  EXPECT_EQ(0, mode.fromArgbColor(other));
}

}  // namespace roo_display
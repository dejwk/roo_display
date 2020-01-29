#include "roo_display/core/color.h"
#include "roo_display/core/color_subpixel.h"
#include "gtest/gtest.h"

namespace roo_display {

// Tests for Monochrome and for Alpha4 in general provide good coverage
// for all sub-pixel color modes that take 1 or 4 bits per pixel.

std::ostream& operator<<(std::ostream& os, const Color& color) {
  return os << "argb:" << std::hex << color.asArgb();
}

TEST(Color, MonochromeLsb) {
  Color fg(0xDAA3E0F5);
  Color bg(0xF1311F3A);
  Monochrome mode(fg, bg);
  SubPixelColorHelper<Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST> subpixel;
  EXPECT_EQ(0xFF, subpixel.RawToFullByte(1));
  EXPECT_EQ(0x00, subpixel.RawToFullByte(0));
  uint8_t target = 0;
  subpixel.applySubPixelColor(1, &target, 0);
  EXPECT_EQ(0x01, target);
  subpixel.applySubPixelColor(1, &target, 6);
  EXPECT_EQ(0x41, target);
  subpixel.applySubPixelColor(0, &target, 0);
  EXPECT_EQ(0x40, target);
  EXPECT_EQ(1, subpixel.ReadSubPixelColor(target, 6));
  EXPECT_EQ(0, subpixel.ReadSubPixelColor(target, 0));

  Color expected[] = { fg, bg, bg, bg, bg, bg, fg, bg };
  Color actual[8];
  subpixel.ReadSubPixelColorBulk(mode, 0x41, actual);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(Color, MonochromeMsb) {
  Color fg(0xDAA3E0F5);
  Color bg(0xF1311F3A);
  Monochrome mode(fg, bg);
  SubPixelColorHelper<Monochrome, COLOR_PIXEL_ORDER_MSB_FIRST> subpixel;
  EXPECT_EQ(0xFF, subpixel.RawToFullByte(1));
  EXPECT_EQ(0x00, subpixel.RawToFullByte(0));
  uint8_t target = 0;
  subpixel.applySubPixelColor(1, &target, 0);
  EXPECT_EQ(0x80, target);
  subpixel.applySubPixelColor(1, &target, 6);
  EXPECT_EQ(0x82, target);
  subpixel.applySubPixelColor(0, &target, 0);
  EXPECT_EQ(0x02, target);
  EXPECT_EQ(1, subpixel.ReadSubPixelColor(target, 6));
  EXPECT_EQ(0, subpixel.ReadSubPixelColor(target, 0));

  Color expected[] = { fg, bg, bg, bg, bg, bg, fg, bg };
  Color actual[8];
  subpixel.ReadSubPixelColorBulk(mode, 0x82, actual);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(Color, Alpha4Lsb) {
  Alpha4 mode(color::Black);
  SubPixelColorHelper<Alpha4, COLOR_PIXEL_ORDER_LSB_FIRST> subpixel;
  EXPECT_EQ(0xCC, subpixel.RawToFullByte(0x0C));
  EXPECT_EQ(0x11, subpixel.RawToFullByte(0x01));
  uint8_t target = 0;
  subpixel.applySubPixelColor(0x0E, &target, 0);
  EXPECT_EQ(0x0E, target);
  subpixel.applySubPixelColor(0x02, &target, 1);
  EXPECT_EQ(0x2E, target);
  subpixel.applySubPixelColor(0x05, &target, 0);
  EXPECT_EQ(0x25, target);
  EXPECT_EQ(0x02, subpixel.ReadSubPixelColor(target, 1));
  EXPECT_EQ(0x05, subpixel.ReadSubPixelColor(target, 0));

  Color expected[] = { 0xEE000000, 0x44000000 };
  Color actual[2];
  subpixel.ReadSubPixelColorBulk(mode, 0x4E, actual);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(Color, Alpha4Msb) {
  Alpha4 mode(color::Black);
  SubPixelColorHelper<Alpha4, COLOR_PIXEL_ORDER_MSB_FIRST> subpixel;
  EXPECT_EQ(0xCC, subpixel.RawToFullByte(0x0C));
  EXPECT_EQ(0x11, subpixel.RawToFullByte(0x01));
  uint8_t target = 0;
  subpixel.applySubPixelColor(0x0E, &target, 0);
  EXPECT_EQ(0xE0, target);
  subpixel.applySubPixelColor(0x02, &target, 1);
  EXPECT_EQ(0xE2, target);
  subpixel.applySubPixelColor(0x05, &target, 0);
  EXPECT_EQ(0x52, target);
  EXPECT_EQ(0x02, subpixel.ReadSubPixelColor(target, 1));
  EXPECT_EQ(0x05, subpixel.ReadSubPixelColor(target, 0));

  Color expected[] = { 0x44000000, 0xEE000000 };
  Color actual[2];
  subpixel.ReadSubPixelColorBulk(mode, 0x4E, actual);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

}  // namespace roo_display
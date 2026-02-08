#include "gtest/gtest.h"
#include "roo_display/color/color.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/named.h"
#include "roo_display/internal/color_io.h"

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
  SubByteColorIo<Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST> io;
  EXPECT_EQ(roo::byte{0xFF}, io.expandRaw(1));
  EXPECT_EQ(roo::byte{0x00}, io.expandRaw(0));
  roo::byte target{0};
  io.storeRaw(1, &target, 0);
  EXPECT_EQ(roo::byte{0x01}, target);
  io.storeRaw(1, &target, 6);
  EXPECT_EQ(roo::byte{0x41}, target);
  io.storeRaw(0, &target, 0);
  EXPECT_EQ(roo::byte{0x40}, target);
  EXPECT_EQ(1, io.loadRaw(target, 6));
  EXPECT_EQ(0, io.loadRaw(target, 0));

  Color expected[] = {fg, bg, bg, bg, bg, bg, fg, bg};
  Color actual[8];
  io.loadBulk(mode, roo::byte{0x41}, actual);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(Color, MonochromeMsb) {
  Color fg(0xDAA3E0F5);
  Color bg(0xF1311F3A);
  Monochrome mode(fg, bg);
  SubByteColorIo<Monochrome, COLOR_PIXEL_ORDER_MSB_FIRST> io;
  EXPECT_EQ(roo::byte{0xFF}, io.expandRaw(1));
  EXPECT_EQ(roo::byte{0x00}, io.expandRaw(0));
  roo::byte target{0};
  io.storeRaw(1, &target, 0);
  EXPECT_EQ(roo::byte{0x80}, target);
  io.storeRaw(1, &target, 6);
  EXPECT_EQ(roo::byte{0x82}, target);
  io.storeRaw(0, &target, 0);
  EXPECT_EQ(roo::byte{0x02}, target);
  EXPECT_EQ(1, io.loadRaw(target, 6));
  EXPECT_EQ(0, io.loadRaw(target, 0));

  Color expected[] = {fg, bg, bg, bg, bg, bg, fg, bg};
  Color actual[8];
  io.loadBulk(mode, roo::byte{0x82}, actual);
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(Color, Alpha4Lsb) {
  Alpha4 mode(color::Black);
  SubByteColorIo<Alpha4, COLOR_PIXEL_ORDER_LSB_FIRST> io;
  EXPECT_EQ(roo::byte{0xCC}, io.expandRaw(0x0C));
  EXPECT_EQ(roo::byte{0x11}, io.expandRaw(0x01));
  roo::byte target{0};
  io.storeRaw(0x0E, &target, 0);
  EXPECT_EQ(roo::byte{0x0E}, target);
  io.storeRaw(0x02, &target, 1);
  EXPECT_EQ(roo::byte{0x2E}, target);
  io.storeRaw(0x05, &target, 0);
  EXPECT_EQ(roo::byte{0x25}, target);
  EXPECT_EQ(0x02, io.loadRaw(target, 1));
  EXPECT_EQ(0x05, io.loadRaw(target, 0));

  Color expected[] = {0xEE000000, 0x44000000};
  Color actual[2];
  io.loadBulk(mode, roo::byte{0x4E}, actual);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

TEST(Color, Alpha4Msb) {
  Alpha4 mode(color::Black);
  SubByteColorIo<Alpha4, COLOR_PIXEL_ORDER_MSB_FIRST> io;
  EXPECT_EQ(roo::byte{0xCC}, io.expandRaw(0x0C));
  EXPECT_EQ(roo::byte{0x11}, io.expandRaw(0x01));
  roo::byte target{0};
  io.storeRaw(0x0E, &target, 0);
  EXPECT_EQ(roo::byte{0xE0}, target);
  io.storeRaw(0x02, &target, 1);
  EXPECT_EQ(roo::byte{0xE2}, target);
  io.storeRaw(0x05, &target, 0);
  EXPECT_EQ(roo::byte{0x52}, target);
  EXPECT_EQ(0x02, io.loadRaw(target, 1));
  EXPECT_EQ(0x05, io.loadRaw(target, 0));

  Color expected[] = {0x44000000, 0xEE000000};
  Color actual[2];
  io.loadBulk(mode, roo::byte{0x4E}, actual);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

}  // namespace roo_display
#include "roo_display/internal/byte_order.h"
#include "gtest/gtest.h"

namespace roo_display {

static const uint16_t i16 = 0x1122;
static const uint32_t i32 = 0x11223344;

using byte_order::Convert;

TEST(ByteOrder, Converts16) {
  EXPECT_EQ(
      0x1122,
      (Convert<uint16_t, BYTE_ORDER_BIG_ENDIAN, BYTE_ORDER_BIG_ENDIAN>(i16)));
  EXPECT_EQ(
      0x1122,
      (Convert<uint16_t, BYTE_ORDER_LITTLE_ENDIAN, BYTE_ORDER_LITTLE_ENDIAN>(
          i16)));
  EXPECT_EQ(0x2211,
            (Convert<uint16_t, BYTE_ORDER_BIG_ENDIAN, BYTE_ORDER_LITTLE_ENDIAN>(
                i16)));
  EXPECT_EQ(0x2211,
            (Convert<uint16_t, BYTE_ORDER_LITTLE_ENDIAN, BYTE_ORDER_BIG_ENDIAN>(
                i16)));
}

TEST(ByteOrder, Converts32) {
  EXPECT_EQ(
      0x11223344,
      (Convert<uint32_t, BYTE_ORDER_BIG_ENDIAN, BYTE_ORDER_BIG_ENDIAN>(i32)));
  EXPECT_EQ(
      0x11223344,
      (Convert<uint32_t, BYTE_ORDER_LITTLE_ENDIAN, BYTE_ORDER_LITTLE_ENDIAN>(
          i32)));
  EXPECT_EQ(0x44332211,
            (Convert<uint32_t, BYTE_ORDER_BIG_ENDIAN, BYTE_ORDER_LITTLE_ENDIAN>(
                i32)));
  EXPECT_EQ(0x44332211,
            (Convert<uint32_t, BYTE_ORDER_LITTLE_ENDIAN, BYTE_ORDER_BIG_ENDIAN>(
                i32)));
}

}  // namespace roo_display
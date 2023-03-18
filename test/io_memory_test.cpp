#include "gtest/gtest.h"

#include "roo_display/io/memory.h"

namespace roo_display {

TEST(DramStream, Reads) {
  uint8_t data[] = {0x23, 0xF5};
  internal::DramStream stream(data);
  EXPECT_EQ(0x23, stream.read());
  EXPECT_EQ(0xF5, stream.read());
}

TEST(DramStream, SkipsAndResets) {
  uint8_t data[] = {0x23, 0xF5, 0xE3, 0x43};
  internal::DramStream stream(data);
  EXPECT_EQ(0x23, stream.read());
  stream.skip(2);
  EXPECT_EQ(0x43, stream.read());
  // stream.seek(1);
  // EXPECT_EQ(0xF5, stream.read());
}

TEST(DramStream, ReadsUint16) {
  uint8_t data[] = {0x23, 0xF5};
  internal::DramStream stream(data);
  EXPECT_EQ(0x23F5, read_uint16_be(&stream));
}

TEST(DramStream, ReadsUint32) {
  uint8_t data[] = {0x23, 0xF5, 0xE3, 0x43};
  internal::DramStream stream(data);
  EXPECT_EQ(0x23F5E343, read_uint32_be(&stream));
}

}  // namespace
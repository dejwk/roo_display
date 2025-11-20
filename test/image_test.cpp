#include "roo_display/image/image.h"

#include "roo_display/color/color.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/image/image_stream.h"
#include "roo_display/io/memory.h"
#include "roo_io/memory/memory_iterable.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(Image, XBitmap) {
  unsigned char test_bits[] = {0x00, 0x00, 0xfe, 0x03, 0xfe, 0x03, 0x1e, 0x00,
                               0x3e, 0x00, 0x76, 0x00, 0x66, 0x00, 0x06, 0x00,
                               0x06, 0x00, 0x06, 0x00, 0x00, 0x00};

  XBitmap<ConstDramPtr> bmp(12, 11, test_bits, color::White, color::Black);
  EXPECT_THAT(bmp, MatchesContent(WhiteOnBlack(), 12, 11,
                                  "            "
                                  " *********  "
                                  " *********  "
                                  " ****       "
                                  " *****      "
                                  " ** ***     "
                                  " **  **     "
                                  " **         "
                                  " **         "
                                  " **         "
                                  "            "));
}

namespace internal {

TEST(BiasedStreamIterator, SingleTransparentPixel) {
  // 0x1 -> 0x0 (a single transparent pixel)
  unsigned char data[] = {0x10};  // 0x1 in upper nibble.

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color pixel = stream.next();
  EXPECT_EQ(pixel.a(), 0x0);
  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(BiasedStreamIterator, RunOfTransparentPixels) {
  // 0x3 -> 0x0 0x0 0x0 (run of 3 transparent pixels)
  unsigned char data[] = {0x30};  // 0x3 in upper nibble

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  ASSERT_EQ(stream.next(), color::Transparent);
  ASSERT_EQ(stream.next(), color::Transparent);
  ASSERT_EQ(stream.next(), color::Transparent);
  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, SingleOpaquePixel) {
  // 0x9 -> 0xF (a single opaque pixel)
  uint8_t data[] = {0x90};  // 0x9 in upper nibble

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  EXPECT_EQ(stream.next(), color::Black);
  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, RunOfOpaquePixels) {
  // 0xD -> 0xF 0xF 0xF 0xF 0xF (run of 5 opaque pixels)
  uint8_t data[] = {0xD0};  // 0xD in upper nibble
  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  ASSERT_EQ(stream.next(), color::Black);
  ASSERT_EQ(stream.next(), color::Black);
  ASSERT_EQ(stream.next(), color::Black);
  ASSERT_EQ(stream.next(), color::Black);
  ASSERT_EQ(stream.next(), color::Black);

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, TwoPixelsOfValue) {
  // 0x0 0x0 0x5 -> 0x5 0x5 (two pixels of value 0x5)
  uint8_t data[] = {0x00, 0x50};  // 0x0, 0x0, 0x5

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color expected = color_mode.toArgbColor(0x5);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, ThreePixelsOfValue) {
  // 0x0 0xF 0x5 -> 0x5 0x5 0x5 (three pixels of value 0x5)
  uint8_t data[] = {0x0F, 0x50};  // 0x0, 0xF, 0x5

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color expected = color_mode.toArgbColor(0x5);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, SingletonValue) {
  // 0x0 0x5 -> single pixel of value 0x5 (where 0x5 != 0x0, 0xF)
  uint8_t data[] = {0x05};  // 0x0, 0x5

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  ASSERT_EQ(stream.next(), color_mode.toArgbColor(0x5));

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, FourPixelsOfValue) {
  // 0x8 0x0 0x0 0x5 -> 0x5 0x5 0x5 0x5 (4 pixels of value 0x5)
  uint8_t data[] = {0x80, 0x05};  // 0x8, 0x0, 0x0, 0x5

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color expected = color_mode.toArgbColor(0x5);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, FivePixelsOfValue) {
  // 0x8 0x0 0x1 0x5 -> 0x5 0x5 0x5 0x5 0x5 (5 pixels of value 0x5)
  uint8_t data[] = {0x80, 0x15};  // 0x8, 0x0, 0x1, 0x5

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color expected = color_mode.toArgbColor(0x5);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);
  ASSERT_EQ(stream.next(), expected);

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, ThreeArbitraryPixels) {
  // 0x8 0x1 0x3 0x4 0x5 -> 0x3 0x4 0x5 (3 arbitrary pixels: 0x3, 0x4, 0x5)
  uint8_t data[] = {0x81, 0x34, 0x50};  // 0x8, 0x1, 0x3, 0x4, 0x5

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  ASSERT_EQ(stream.next(), color_mode.toArgbColor(0x3));
  ASSERT_EQ(stream.next(), color_mode.toArgbColor(0x4));
  ASSERT_EQ(stream.next(), color_mode.toArgbColor(0x5));

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, VarintArbitraryPixels) {
  // 0x8 0xD 0x1 ... -> 13 (11 + 2) arbitrary pixels that follow
  uint8_t data[] = {0x8D, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE};
  // 0x8, varint(0xD,0x1) = 13, then 13 arbitrary pixels

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  // Should read 13 arbitrary pixels
  uint8_t expected_values[] = {0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
                               0x9, 0xA, 0xB, 0xC, 0xD, 0xE};

  for (int i = 0; i < 13; i++) {
    ASSERT_EQ(stream.next(), color_mode.toArgbColor(expected_values[i]));
  }

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, ReadAndSkipMethods) {
  // Test Read() and Skip() methods
  uint8_t data[] = {0x50};  // Run of 5 transparent pixels

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color buffer[3];
  stream.Read(buffer, 3);

  // All should be transparent.
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(buffer[i].a(), 0x0);
  }

  // Skip remaining 2 pixels
  stream.Skip(2);

  ASSERT_TRUE(stream.ok());
  stream.next();
  ASSERT_FALSE(stream.ok());
}

TEST(RleStream4bppxBiased, TransparencyMode) {
  uint8_t data[] = {0x10};

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  ASSERT_EQ(stream.transparency(), color_mode.transparency());
}

}  // namespace internal
}  // namespace roo_display

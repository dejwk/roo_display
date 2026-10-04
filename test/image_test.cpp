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
  // Test read() and skip() methods
  uint8_t data[] = {0x50};  // Run of 5 transparent pixels

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Alpha4 color_mode(color::Black);
  RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
      resource.iterator(), color_mode);

  Color buffer[3];
  stream.read(buffer, 3);

  // All should be transparent.
  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(buffer[i].a(), 0x0);
  }

  // Skip remaining 2 pixels
  stream.skip(2);

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

TEST(RleStreamUniform, ReportsExactRunLengthForByteAlignedRuns) {
  uint8_t data[] = {0x82, 0x12, 0x34};  // Run of 3 RGB565 pixels.

  roo_io::MemoryIterable resource((const roo::byte*)data,
                                  (const roo::byte*)(data + sizeof(data)));
  Rgb565 color_mode;
  RleStreamUniform<roo_io::MemoryIterable, Rgb565> stream(resource.iterator(),
                                                          color_mode);

  Color buf[2];
  uint32_t run_length = 0;
  stream.read(buf, 2, run_length);
  EXPECT_EQ(run_length, 3u);
  EXPECT_EQ(buf[0], buf[1]);

  stream.read(buf, 1, run_length);
  EXPECT_EQ(run_length, 1u);
}

TEST(RleStreamUniform, ReportsSubByteRunLengthOnlyForUniformPattern) {
  {
    uint8_t data[] = {0x81, 0x11};  // Run of 4 Alpha4 pixels with value 0x1.
    roo_io::MemoryIterable resource((const roo::byte*)data,
                                    (const roo::byte*)(data + sizeof(data)));
    Alpha4 color_mode(color::Black);
    RleStreamUniform<roo_io::MemoryIterable, Alpha4> stream(resource.iterator(),
                                                            color_mode);

    Color buf[2];
    uint32_t run_length = 0;
    stream.read(buf, 2, run_length);
    EXPECT_EQ(run_length, 4u);
  }

  {
    uint8_t data[] = {0x81, 0x12};  // Run with alternating Alpha4 values.
    roo_io::MemoryIterable resource((const roo::byte*)data,
                                    (const roo::byte*)(data + sizeof(data)));
    Alpha4 color_mode(color::Black);
    RleStreamUniform<roo_io::MemoryIterable, Alpha4> stream(resource.iterator(),
                                                            color_mode);

    Color buf[2];
    uint32_t run_length = 0;
    stream.read(buf, 2, run_length);
    EXPECT_EQ(run_length, 0u);
  }
}

// Checks every packed byte, including reads starting within its repeated
// pattern.
template <typename ColorMode>
void CheckPackedRunPatterns(const ColorMode& mode) {
  constexpr int kBits = ColorMode::bits_per_pixel;
  constexpr int kPixelsPerByte = 8 / kBits;
  for (int pattern = 0; pattern < 256; ++pattern) {
    for (int groups : {1, 3, 64}) {
      const uint8_t data[] = {static_cast<uint8_t>(0x80 | (groups - 1)),
                              static_cast<uint8_t>(pattern)};
      for (int batch : {1, 3, 5, 64}) {
        RleStreamUniform<ConstDramPtr, ColorMode> stream(
            ConstDramPtr(data).iterator(), mode);
        std::vector<Color> expected(groups * kPixelsPerByte);
        for (size_t i = 0; i < expected.size(); ++i) {
          int shift = 8 - kBits * (i % kPixelsPerByte + 1);
          expected[i] =
              mode.toArgbColor((pattern >> shift) & ((1 << kBits) - 1));
        }
        for (size_t offset = 0; offset < expected.size();) {
          uint32_t remaining = expected.size() - offset;
          uint32_t expected_run = remaining;
          for (size_t i = offset + 1; i < expected.size(); ++i) {
            if (expected[i] != expected[offset]) {
              expected_run = 0;
              break;
            }
          }
          uint16_t count = std::min<uint32_t>(batch, remaining);
          Color pixels[64];
          uint32_t run = 0;
          stream.read(pixels, count, run);
          ASSERT_EQ(run, expected_run) << pattern << "/" << offset;
          for (uint16_t i = 0; i < count; ++i) {
            ASSERT_EQ(pixels[i], expected[offset + i]);
          }
          offset += count;
        }
      }
    }
  }
}

// Verifies exact run promises and pixels for 1-, 2-, and 4-bit repeated bytes,
// including nonuniform patterns and tails shorter than a complete byte.
TEST(RleStreamUniform, PackedRunPatternsAndPartialTails) {
  CheckPackedRunPatterns(Monochrome(color::White, color::Black));
  const Color colors[] = {color::Black, color::Red, color::Green, color::Blue};
  Palette palette = Palette::ReadOnly(colors, 4);
  CheckPackedRunPatterns(Indexed2(&palette));
  CheckPackedRunPatterns(Alpha4(color::Blue));
}

// Verifies repeated small reads keep the exact remaining length of a long run.
TEST(RleStreamUniform, LongPackedRunInSmallBatches) {
  const uint8_t data[] = {0xC1, 0xFF, 0x7F, 0x88};  // 65536 Alpha4 pixels.
  RleStreamUniform<ConstDramPtr, Alpha4> stream(ConstDramPtr(data).iterator(),
                                                Alpha4(color::Blue));
  for (uint32_t remaining = 65536; remaining > 0; remaining -= 64) {
    Color pixels[64];
    uint32_t run = 0;
    stream.read(pixels, 64, run);
    ASSERT_EQ(run, remaining);
    for (Color pixel : pixels) EXPECT_EQ(pixel, color::Blue.withA(0x88));
  }
}

TEST(RleStream4bppxBiased, ReportsRunLengthOnlyForRunGroups) {
  {
    uint8_t data[] = {0xD0};  // Run of 5 opaque pixels.
    roo_io::MemoryIterable resource((const roo::byte*)data,
                                    (const roo::byte*)(data + sizeof(data)));
    Alpha4 color_mode(color::Black);
    RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
        resource.iterator(), color_mode);

    Color buf[2];
    uint32_t run_length = 0;
    stream.read(buf, 2, run_length);
    EXPECT_EQ(run_length, 5u);
  }

  {
    uint8_t data[] = {0x81, 0x34, 0x50};  // 3 arbitrary values.
    roo_io::MemoryIterable resource((const roo::byte*)data,
                                    (const roo::byte*)(data + sizeof(data)));
    Alpha4 color_mode(color::Black);
    RleStream4bppxBiased<roo_io::MemoryIterable, Alpha4> stream(
        resource.iterator(), color_mode);

    Color buf[3];
    uint32_t run_length = 0;
    stream.read(buf, 3, run_length);
    EXPECT_EQ(run_length, 0u);
  }
}

// Compares every skip offset against decoding the same pixels normally.
template <typename Stream, typename ColorMode>
void CheckRleSkips(const std::vector<uint8_t>& bytes, const ColorMode& mode,
                   int count) {
  for (int prefix = 0; prefix <= std::min(count, 5); ++prefix) {
    for (int skipped = 0; skipped <= count - prefix; ++skipped) {
      Stream actual(ConstDramPtr(bytes.data()).iterator(), mode);
      Stream expected(ConstDramPtr(bytes.data()).iterator(), mode);
      for (int i = 0; i < prefix; ++i)
        EXPECT_EQ(actual.next(), expected.next());
      actual.skip(skipped);
      for (int i = 0; i < skipped; ++i) expected.next();
      int remaining = count - prefix - skipped;
      std::vector<Color> result(remaining);
      std::vector<Color> reference(remaining);
      uint32_t run = 0;
      uint32_t expected_run = 0;
      actual.read(result.data(), remaining, run);
      expected.read(reference.data(), remaining, expected_run);
      EXPECT_EQ(run, expected_run) << prefix << "/" << skipped;
      EXPECT_EQ(result, reference) << prefix << "/" << skipped;
    }
  }
}

// Verifies skips across literal/run groups and cached packed-byte boundaries.
TEST(RleStreamUniform, SkipAcrossEncodedGroups) {
  CheckRleSkips<RleStreamUniform<ConstDramPtr, Alpha8>>(
      {0x03, 0x10, 0x20, 0x30, 0x40, 0x84, 0x90, 0x01, 0x40, 0xff},
      Alpha8(color::Red), 11);
  CheckRleSkips<RleStreamUniform<ConstDramPtr, Rgb565>>(
      {0x02, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0x82, 0xcd, 0xef}, Rgb565(),
      6);
  const std::vector<uint8_t> packed = {0x02, 0x12, 0x34, 0x56, 0x83,
                                       0x89, 0x01, 0xab, 0xcd};
  CheckRleSkips<RleStreamUniform<ConstDramPtr, Alpha4>>(
      packed, Alpha4(color::Blue), 18);
  CheckRleSkips<RleStreamUniform<ConstDramPtr, Monochrome>>(
      packed, Monochrome(color::White), 72);
  const Color colors[] = {color::Black, color::Red, color::Green, color::Blue};
  Palette palette = Palette::ReadOnly(colors, 4);
  CheckRleSkips<RleStreamUniform<ConstDramPtr, Indexed2>>(
      packed, Indexed2(&palette), 36);
}

// Verifies biased runs and literal nibbles preserve odd/even cursor alignment.
TEST(RleStream4bppxBiased, SkipAcrossEncodedGroups) {
  CheckRleSkips<RleStream4bppxBiased<ConstDramPtr, Alpha4>>(
      {0x81, 0x34, 0x5d, 0x00, 0x78, 0x01, 0x98, 0x21, 0x23, 0x40},
      Alpha4(color::Blue), 19);
}

class CountingAlpha8 : public Alpha8 {
 public:
  explicit CountingAlpha8(int* conversions)
      : Alpha8(color::Blue), conversions_(conversions) {}

  Color toArgbColor(uint8_t value) const {
    ++*conversions_;
    return Alpha8::toArgbColor(value);
  }

 private:
  int* conversions_;
};

// Verifies literal skips do no color conversion and long runs decode once.
TEST(RleStreamUniform, SkipDoesNotDecodeDiscardedPixels) {
  int conversions = 0;
  std::vector<uint8_t> bytes(65, 128);
  bytes[0] = 0x3f;  // 64 literal pixels, then a 65536-pixel run.
  bytes.insert(bytes.end(), {0xc3, 0xff, 0x7f, 192});
  RleStreamUniform<ConstDramPtr, CountingAlpha8> stream(
      ConstDramPtr(bytes.data()).iterator(), CountingAlpha8(&conversions));
  stream.skip(0);
  EXPECT_EQ(conversions, 0);
  stream.skip(64);
  EXPECT_EQ(conversions, 0);
  stream.skip(65535);
  EXPECT_EQ(conversions, 1);
  EXPECT_EQ(stream.next(), color::Blue.withA(192));
  EXPECT_EQ(conversions, 1);
}

}  // namespace internal
// Verifies 8-bit RLE accepts byte-returning memory iterators and preserves
// decoded alpha values and run metadata through source clipping.
TEST(Image, RleAlpha8MemoryStream) {
  const uint8_t data[] = {0x83, 128, 0x82, 32};
  RleImage<Alpha8, ConstDramPtr> image(7, 1, data, Alpha8(color::Red));
  Color pixels[7];
  uint32_t run = 0;
  auto stream = image.createStream();
  stream->read(pixels, 7, run);
  EXPECT_EQ(run, 4u);
  for (int i = 0; i < 7; ++i) {
    EXPECT_EQ(pixels[i], color::Red.withA(i < 4 ? 128 : 32));
  }
  stream = image.createStream(Box(2, 0, 5, 0));
  stream->read(pixels, 4, run);
  EXPECT_EQ(run, 2u);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(pixels[i], color::Red.withA(i < 2 ? 128 : 32));
  }
}

}  // namespace roo_display

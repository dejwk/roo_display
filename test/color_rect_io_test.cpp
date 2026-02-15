#include <vector>

#include "gtest/gtest.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/named.h"
#include "roo_display/internal/color_io.h"

namespace roo_display {

namespace {

template <typename ColorMode, roo_io::ByteOrder byte_order,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
std::vector<roo::byte> MakeRectData(const std::vector<Color>& colors,
                                    int16_t width, int16_t height,
                                    size_t row_width_bytes,
                                    const ColorMode& mode = ColorMode()) {
  std::vector<roo::byte> data(row_width_bytes * height, roo::byte{0});
  if constexpr (ColorTraits<ColorMode>::pixels_per_byte == 1) {
    ColorIo<ColorMode, byte_order> io;
    for (int16_t y = 0; y < height; ++y) {
      roo::byte* row = data.data() + y * row_width_bytes;
      for (int16_t x = 0; x < width; ++x) {
        io.store(colors[y * width + x],
                 row + x * ColorTraits<ColorMode>::bytes_per_pixel, mode);
      }
    }
  } else {
    SubByteColorIo<ColorMode, pixel_order> io;
    for (int16_t y = 0; y < height; ++y) {
      roo::byte* row = data.data() + y * row_width_bytes;
      for (int16_t x = 0; x < width; ++x) {
        roo::byte* target = row + x / ColorTraits<ColorMode>::pixels_per_byte;
        io.storeRaw(mode.fromArgbColor(colors[y * width + x]), target,
                    x % ColorTraits<ColorMode>::pixels_per_byte);
      }
    }
  }
  return data;
}

template <typename ColorMode, roo_io::ByteOrder byte_order,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
std::vector<Color> ExpectRect(const roo::byte* data, size_t row_width_bytes,
                              int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              const ColorMode& mode = ColorMode()) {
  std::vector<Color> expected;
  expected.reserve((x1 - x0 + 1) * (y1 - y0 + 1));
  if constexpr (ColorTraits<ColorMode>::pixels_per_byte == 1) {
    ColorIo<ColorMode, byte_order> io;
    constexpr size_t bpp = ColorTraits<ColorMode>::bytes_per_pixel;
    for (int16_t y = y0; y <= y1; ++y) {
      const roo::byte* row = data + y * row_width_bytes + x0 * bpp;
      for (int16_t x = x0; x <= x1; ++x) {
        expected.push_back(io.load(row, mode));
        row += bpp;
      }
    }
  } else {
    SubByteColorIo<ColorMode, pixel_order> io;
    const int8_t ppb = ColorTraits<ColorMode>::pixels_per_byte;
    for (int16_t y = y0; y <= y1; ++y) {
      const roo::byte* row = data + y * row_width_bytes;
      for (int16_t x = x0; x <= x1; ++x) {
        uint8_t raw = io.loadRaw(row[x / ppb], x % ppb);
        expected.push_back(mode.toArgbColor(raw));
      }
    }
  }
  return expected;
}

std::vector<Color> MakeColors(int16_t width, int16_t height, uint32_t seed) {
  std::vector<Color> colors(width * height);
  uint32_t value = seed;
  for (int i = 0; i < width * height; ++i) {
    value = value * 1664525u + 1013904223u;
    colors[i] = Color(0xFF000000 | (value & 0x00FFFFFF));
  }
  return colors;
}

}  // namespace

TEST(ColorRectIo, Rgb565Contiguous) {
  constexpr int16_t width = 4;
  constexpr int16_t height = 2;
  constexpr size_t row_width_bytes = width * 2;

  auto colors = MakeColors(width, height, 0x1A2B3C4D);

  auto data = MakeRectData<Rgb565, roo_io::kNativeEndian>(colors, width, height,
                                                          row_width_bytes);

  std::vector<Color> output(width * height);
  ColorRectIo<Rgb565, roo_io::kNativeEndian> rect_io;
  rect_io.decode(data.data(), row_width_bytes, 0, 0, width - 1, height - 1,
                    output.data());

  auto expected = ExpectRect<Rgb565, roo_io::kNativeEndian>(
      data.data(), row_width_bytes, 0, 0, width - 1, height - 1);
  EXPECT_EQ(expected, output);
}

TEST(ColorRectIo, Rgb565PaddedRowsSubrect) {
  constexpr int16_t width = 5;
  constexpr int16_t height = 3;
  constexpr size_t row_width_bytes = width * 2 + 2;

  auto colors = MakeColors(width, height, 0x12345678);
  auto data = MakeRectData<Rgb565, roo_io::kNativeEndian>(colors, width, height,
                                                          row_width_bytes);

  std::vector<Color> output(3 * 2);
  ColorRectIo<Rgb565, roo_io::kNativeEndian> rect_io;
  rect_io.decode(data.data(), row_width_bytes, 1, 1, 3, 2, output.data());

  auto expected = ExpectRect<Rgb565, roo_io::kNativeEndian>(
      data.data(), row_width_bytes, 1, 1, 3, 2);
  EXPECT_EQ(expected, output);
}

TEST(ColorRectIo, Rgb565MultiLineLarge) {
  constexpr int16_t width = 17;
  constexpr int16_t height = 11;
  constexpr size_t row_width_bytes = width * 2 + 6;
  auto colors = MakeColors(width, height, 0xCAFEBABE);
  auto data = MakeRectData<Rgb565, roo_io::kNativeEndian>(colors, width, height,
                                                          row_width_bytes);

  std::vector<Color> output(12 * 7);
  ColorRectIo<Rgb565, roo_io::kNativeEndian> rect_io;
  rect_io.decode(data.data(), row_width_bytes, 3, 2, 14, 8, output.data());

  auto expected = ExpectRect<Rgb565, roo_io::kNativeEndian>(
      data.data(), row_width_bytes, 3, 2, 14, 8);
  EXPECT_EQ(expected, output);
}

TEST(ColorRectIo, Grayscale4ContiguousFullRows) {
  Grayscale4 mode;
  ColorRectIo<Grayscale4, roo_io::kNativeEndian, COLOR_PIXEL_ORDER_LSB_FIRST>
      rect_io;

  constexpr int16_t width = 8;
  constexpr int16_t height = 2;
  constexpr size_t row_width_bytes = 4;
  auto colors = MakeColors(width, height, 0xA1B2C3D4);
  auto data = MakeRectData<Grayscale4, roo_io::kNativeEndian,
                           COLOR_PIXEL_ORDER_LSB_FIRST>(colors, width, height,
                                                        row_width_bytes, mode);

  std::vector<Color> output(width * height);
  rect_io.decode(data.data(), row_width_bytes, 0, 0, width - 1, height - 1,
                    output.data(), mode);

  auto expected = ExpectRect<Grayscale4, roo_io::kNativeEndian,
                             COLOR_PIXEL_ORDER_LSB_FIRST>(
      data.data(), row_width_bytes, 0, 0, width - 1, height - 1, mode);
  EXPECT_EQ(expected, output);
}

TEST(ColorRectIo, Grayscale4Misaligned) {
  Grayscale4 mode;
  ColorRectIo<Grayscale4, roo_io::kNativeEndian, COLOR_PIXEL_ORDER_LSB_FIRST>
      rect_io;

  constexpr int16_t width = 10;
  constexpr int16_t height = 4;
  constexpr size_t row_width_bytes = 5;
  auto colors = MakeColors(width, height, 0x11223344);
  auto data = MakeRectData<Grayscale4, roo_io::kNativeEndian,
                           COLOR_PIXEL_ORDER_LSB_FIRST>(colors, width, height,
                                                        row_width_bytes, mode);

  std::vector<Color> output(8 * 3);
  rect_io.decode(data.data(), row_width_bytes, 1, 1, 8, 3, output.data(),
                    mode);

  auto expected = ExpectRect<Grayscale4, roo_io::kNativeEndian,
                             COLOR_PIXEL_ORDER_LSB_FIRST>(
      data.data(), row_width_bytes, 1, 1, 8, 3, mode);
  EXPECT_EQ(expected, output);
}

TEST(ColorRectIo, Grayscale4Aligned) {
  Grayscale4 mode;
  ColorRectIo<Grayscale4, roo_io::kNativeEndian, COLOR_PIXEL_ORDER_LSB_FIRST>
      rect_io;

  constexpr int16_t width = 6;
  constexpr int16_t height = 11;
  constexpr size_t row_width_bytes = 3;
  auto colors = MakeColors(width, height, 0x55667788);
  auto data = MakeRectData<Grayscale4, roo_io::kNativeEndian,
                           COLOR_PIXEL_ORDER_LSB_FIRST>(colors, width, height,
                                                        row_width_bytes, mode);

  std::vector<Color> output(4 * 2);
  rect_io.decode(data.data(), row_width_bytes, 2, 3, 5, 4, output.data(),
                    mode);

  auto expected = ExpectRect<Grayscale4, roo_io::kNativeEndian,
                             COLOR_PIXEL_ORDER_LSB_FIRST>(
      data.data(), row_width_bytes, 2, 3, 5, 4, mode);
  EXPECT_EQ(expected, output);
}

TEST(ColorRectIo, Grayscale4MultiLinePadded) {
  Grayscale4 mode;
  constexpr int16_t width = 13;
  constexpr int16_t height = 7;
  constexpr size_t row_width_bytes = 8;
  auto colors = MakeColors(width, height, 0x0BADF00D);
  auto data = MakeRectData<Grayscale4, roo_io::kNativeEndian,
                           COLOR_PIXEL_ORDER_LSB_FIRST>(colors, width, height,
                                                        row_width_bytes, mode);

  std::vector<Color> output(6 * 4);
  ColorRectIo<Grayscale4, roo_io::kNativeEndian, COLOR_PIXEL_ORDER_LSB_FIRST>
      rect_io;
  rect_io.decode(data.data(), row_width_bytes, 3, 2, 8, 5, output.data(),
                    mode);

  auto expected = ExpectRect<Grayscale4, roo_io::kNativeEndian,
                             COLOR_PIXEL_ORDER_LSB_FIRST>(
      data.data(), row_width_bytes, 3, 2, 8, 5, mode);
  EXPECT_EQ(expected, output);
}

}  // namespace roo_display

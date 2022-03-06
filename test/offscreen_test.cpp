
#include "roo_display/core/offscreen.h"

#include <memory>
#include <random>

#include "gtest/gtest-param-test.h"
#include "roo_display.h"
#include "roo_display/core/color.h"
#include "roo_display/core/streamable.h"
#include "roo_display/internal/color_subpixel.h"
#include "testing_display_device.h"

using namespace testing;

// static std::default_random_engine generator;

namespace roo_display {

// Trivial, byte-order-respecting, raw color reader, returning storage_type.
template <ByteOrder byte_order>
uint32_t read_bytes(const uint8_t* p, int count);

template <>
uint32_t read_bytes<BYTE_ORDER_BIG_ENDIAN>(const uint8_t* p, int count) {
  uint32_t result = 0;
  while (count-- > 0) {
    result <<= 8;
    result |= *p++;
  }
  return result;
}

template <>
uint32_t read_bytes<BYTE_ORDER_LITTLE_ENDIAN>(const uint8_t* p, int count) {
  uint32_t result = 0;
  p += count - 1;
  while (count-- > 0) {
    result <<= 8;
    result |= *p--;
  }
  return result;
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type = ColorStorageType<ColorMode>,
          int bytes_per_pixel = ColorTraits<ColorMode>::bytes_per_pixel,
          int pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          bool sub_pixel = (ColorTraits<ColorMode>::pixels_per_byte > 1)>
class RawColorReader {
 public:
  RawColorReader(const uint8_t* data, Box extents, ColorMode color_mode)
      : color_mode_(color_mode), data_(data), extents_(extents) {}

  Color get(int16_t x, int16_t y) const {
    int size = ColorMode::bits_per_pixel / 8;
    uint32_t offset =
        (x - extents_.xMin() + (y - extents_.yMin()) * extents_.width());
    const uint8_t* ptr = data_ + (offset * size);
    return color_mode_.toArgbColor(read_bytes<byte_order>(ptr, size));
  }

 private:
  ColorMode color_mode_;
  const uint8_t* data_;
  Box extents_;
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int pixels_per_byte>
class RawColorReader<ColorMode, pixel_order, byte_order, uint8_t, 1,
                     pixels_per_byte, true> {
 public:
  RawColorReader(const uint8_t* data, Box extents, ColorMode color_mode,
                 int pixel_index = 0)
      : color_mode_(color_mode),
        data_(data),
        extents_(extents),
        pixel_index_(pixel_index) {}

  Color get(int16_t x, int16_t y) const {
    uint32_t offset =
        (x - extents_.xMin() + (y - extents_.yMin()) * extents_.width());
    SubPixelColorHelper<ColorMode, pixel_order> helper;
    int32_t byte_offset = (offset + pixel_index_) / pixels_per_byte;
    int pixel_idx = (offset + pixel_index_) % pixels_per_byte;
    return color_mode_.toArgbColor(
        helper.ReadSubPixelColor(data_[byte_offset], pixel_idx));
  }

 private:
  ColorMode color_mode_;
  const uint8_t* data_;
  Box extents_;
  int pixel_index_;
};

class TrivialColorReader {
 public:
  template <typename ColorMode>
  TrivialColorReader(const Color* colors, Box extents, ColorMode mode)
      : colors_(colors), extents_(extents) {}

  Color get(int16_t x, int16_t y) const {
    uint32_t offset =
        (x - extents_.xMin() + (y - extents_.yMin()) * extents_.width());
    return colors_[offset];
  }

 private:
  const Color* colors_;
  Box extents_;
};

template <typename Reader>
class ColorStream : public PixelStream {
 public:
  template <typename... Args>
  ColorStream(Box bounds, Args&&... args)
      : reader_(std::forward<Args>(args)...),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()) {}

  void Read(Color* buf, uint16_t size) override {
    for (int i = 0; i < size; ++i) {
      buf[i++] = next();
    }
  }

  void Skip(uint32_t count) override {
    auto w = bounds_.width();
    y_ += count / w;
    x_ += count % w;
    if (x_ > bounds_.xMax()) {
      x_ -= w;
      ++y_;
    }
  }

  Color next() {
    Color result = reader_.get(x_, y_);
    if (x_ < bounds_.xMax()) {
      ++x_;
    } else {
      x_ = bounds_.xMin();
      ++y_;
    }
    return result;
  }

 private:
  Reader reader_;
  Box bounds_;
  int16_t x_, y_;
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
class RawColorRect : public Streamable {
 public:
  typedef RawColorReader<ColorMode, pixel_order, byte_order> Reader;

  RawColorRect(int16_t width, int16_t height, const uint8_t* data,
               const ColorMode& color_mode = ColorMode())
      : extents_(Box(0, 0, width - 1, height - 1)),
        data_(data),
        color_mode_(color_mode) {}

  std::unique_ptr<PixelStream> CreateStream() const override {
    return std::unique_ptr<PixelStream>(
        new ColorStream<Reader>(extents_, data_, extents_, color_mode_));
  }

  std::unique_ptr<PixelStream> CreateStream(const Box& bounds) const override {
    return std::unique_ptr<PixelStream>(
        new ColorStream<Reader>(bounds, data_, extents_, color_mode_));
  }

  std::unique_ptr<ColorStream<Reader>> CreateRawStream() const {
    return std::unique_ptr<ColorStream<Reader>>(
        new ColorStream<Reader>(extents_, data_, extents_, color_mode_));
  }

  Box extents() const override { return extents_; }

  const ColorMode& color_mode() const { return color_mode_; }

 private:
  Box extents_;
  const uint8_t* data_;
  ColorMode color_mode_;
};

template <typename ColorMode>
class TrivialColorRect : public Streamable {
 public:
  TrivialColorRect(int16_t width, int16_t height, const Color* colors,
                   const ColorMode& color_mode = ColorMode())
      : extents_(Box(0, 0, width - 1, height - 1)),
        colors_(colors),
        color_mode_(color_mode) {}

  std::unique_ptr<PixelStream> CreateStream() const override {
    return std::unique_ptr<PixelStream>(new ColorStream<TrivialColorReader>(
        extents_, colors_, extents_, color_mode_));
  }

  std::unique_ptr<PixelStream> CreateStream(const Box& bounds) const override {
    return std::unique_ptr<PixelStream>(new ColorStream<TrivialColorReader>(
        bounds, colors_, extents_, color_mode_));
  }

  std::unique_ptr<ColorStream<TrivialColorReader>> CreateRawStream() const {
    return std::unique_ptr<ColorStream<TrivialColorReader>>(
        new ColorStream<TrivialColorReader>(extents_, colors_, extents_,
                                            color_mode_));
  }

  Box extents() const override { return extents_; }

  const ColorMode& color_mode() const { return color_mode_; }

 private:
  Box extents_;
  const Color* colors_;
  ColorMode color_mode_;
};

template <typename ColorMode>
class TrivialReplaceWriter {
 public:
  TrivialReplaceWriter(const ColorMode& color_mode, const Color* color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(Color* p, uint32_t offset) { operator()(p, offset, 1); }

  void operator()(Color* p, uint32_t offset, uint32_t count) {
    while (count-- > 0) {
      p[offset++] =
          color_mode_.toArgbColor(color_mode_.fromArgbColor(*color_++));
    }
  }

 private:
  const ColorMode& color_mode_;
  const Color* color_;
};

template <typename ColorMode>
class TrivialBlendWriter {
 public:
  TrivialBlendWriter(const ColorMode& color_mode, const Color* color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(Color* p, uint32_t offset) { operator()(p, offset, 1); }

  void operator()(Color* p, uint32_t offset, uint32_t count) {
    while (count-- > 0) {
      Color bg = p[offset];
      p[offset++] = color_mode_.toArgbColor(
          color_mode_.fromArgbColor(alphaBlend(bg, *color_++)));
    }
  }

 private:
  const ColorMode& color_mode_;
  const Color* color_;
};

template <typename ColorMode>
class TrivialReplaceFiller {
 public:
  TrivialReplaceFiller(const ColorMode& color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(Color* p, uint32_t offset) { operator()(p, offset, 1); }

  void operator()(Color* p, uint32_t offset, uint32_t count) {
    while (count-- > 0) {
      p[offset++] = color_mode_.toArgbColor(color_mode_.fromArgbColor(color_));
    }
  }

 private:
  const ColorMode& color_mode_;
  Color color_;
};

template <typename ColorMode>
class TrivialBlendFiller {
 public:
  TrivialBlendFiller(const ColorMode& color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(Color* p, uint32_t offset) { operator()(p, offset, 1); }

  void operator()(Color* p, uint32_t offset, uint32_t count) {
    while (count-- > 0) {
      Color bg = p[offset];
      p[offset++] = color_mode_.toArgbColor(
          color_mode_.fromArgbColor(alphaBlend(bg, color_)));
    }
  }

 private:
  const ColorMode& color_mode_;
  Color color_;
};

// ReplaceWriter

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
class WriterTester {
 public:
  WriterTester<ColorMode, pixel_order, byte_order>(int16_t width,
                                                   int16_t height,
                                                   ColorMode color_mode)
      : color_mode_(color_mode),
        width_(width),
        height_(height),
        actual_(
            new uint8_t[(width * height * ColorMode::bits_per_pixel + 7) / 8]),
        expected_(new Color[width * height]) {
    Color bg = color_mode_.toArgbColor(0);
    for (int32_t i = 0;
         i < (width * height * ColorMode::bits_per_pixel + 7) / 8; ++i)
      actual_[i] = 0;
    for (int32_t i = 0; i < width * height; ++i) expected_[i] = bg;
  }

  void write(PaintMode mode, Color* colors, uint32_t offset) {
    switch (mode) {
      case PAINT_MODE_REPLACE: {
        internal::ReplaceWriter<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, colors);
        TrivialReplaceWriter<ColorMode> tester_(color_mode_, colors);
        tested_(actual_.get(), offset);
        tester_(expected_.get(), offset);
        break;
      }
      case PAINT_MODE_BLEND: {
        internal::BlendWriter<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, colors);
        TrivialBlendWriter<ColorMode> tester_(color_mode_, colors);
        tested_(actual_.get(), offset);
        tester_(expected_.get(), offset);
        break;
      }
      default: {
        assert(false);
      }
    }
    CheckEq();
  }

  void write(PaintMode mode, Color* colors, uint32_t offset, uint32_t count) {
    switch (mode) {
      case PAINT_MODE_REPLACE: {
        internal::ReplaceWriter<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, colors);
        TrivialReplaceWriter<ColorMode> tester_(color_mode_, colors);
        tested_(actual_.get(), offset, count);
        tester_(expected_.get(), offset, count);
        break;
      }
      case PAINT_MODE_BLEND: {
        internal::BlendWriter<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, colors);
        TrivialBlendWriter<ColorMode> tester_(color_mode_, colors);
        tested_(actual_.get(), offset, count);
        tester_(expected_.get(), offset, count);
        break;
      }
      default: {
        assert(false);
      }
    }
    CheckEq();
  }

  void fill(PaintMode mode, Color color, uint32_t offset) {
    switch (mode) {
      case PAINT_MODE_REPLACE: {
        internal::ReplaceFiller<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, color);
        TrivialReplaceFiller<ColorMode> tester_(color_mode_, color);
        tested_(actual_.get(), offset);
        tester_(expected_.get(), offset);
        break;
      }
      case PAINT_MODE_BLEND: {
        internal::BlendFiller<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, color);
        TrivialBlendFiller<ColorMode> tester_(color_mode_, color);
        tested_(actual_.get(), offset);
        tester_(expected_.get(), offset);
        break;
      }
      default: {
        assert(false);
      }
    }
    CheckEq();
  }

  void fill(PaintMode mode, Color color, uint32_t offset, uint32_t count) {
    switch (mode) {
      case PAINT_MODE_REPLACE: {
        internal::ReplaceFiller<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, color);
        TrivialReplaceFiller<ColorMode> tester_(color_mode_, color);
        tested_(actual_.get(), offset, count);
        tester_(expected_.get(), offset, count);
        break;
      }
      case PAINT_MODE_BLEND: {
        internal::BlendFiller<ColorMode, pixel_order, byte_order> tested_(
            color_mode_, color);
        TrivialBlendFiller<ColorMode> tester_(color_mode_, color);
        tested_(actual_.get(), offset, count);
        tester_(expected_.get(), offset, count);
        break;
      }
      default: {
        assert(false);
      }
    }
    CheckEq();
  }

 private:
  void CheckEq() const {
    RawColorRect<ColorMode, pixel_order, byte_order> actual(
        width_, height_, actual_.get(), color_mode_);
    TrivialColorRect<ColorMode> expected(width_, height_, expected_.get(),
                                         color_mode_);
    EXPECT_THAT(actual, MatchesContent(expected));
  }

  ColorMode color_mode_;
  int16_t width_;
  int16_t height_;
  std::unique_ptr<uint8_t[]> actual_;
  std::unique_ptr<Color[]> expected_;
};

template <typename ColorMode>
class ColorRandomizer {
 public:
  ColorRandomizer(int size, ColorMode color_mode = ColorMode())
      : color_mode_(color_mode), distribution_(), colors_(new Color[size]) {
    for (int i = 0; i < size; ++i) {
      colors_[i] = next();
    }
  }

  Color* get() { return colors_.get(); }

 private:
  // Randomly choses a color from the target's color mode space.
  Color next() { return color_mode_.toArgbColor(distribution_(generator)); }

  ColorMode color_mode_;
  std::uniform_int_distribution<ColorStorageType<ColorMode>> distribution_;
  std::unique_ptr<Color[]> colors_;
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
void TestWriteBasic(PaintMode paint_mode, ColorMode color_mode = ColorMode()) {
  ColorRandomizer<ColorMode> colors(32, color_mode);
  WriterTester<ColorMode, pixel_order, byte_order> tester(4, 8, color_mode);
  tester.write(paint_mode, colors.get() + 0, 0);
  tester.write(paint_mode, colors.get() + 1, 3);
  tester.write(paint_mode, colors.get() + 2, 11);
  tester.write(paint_mode, colors.get() + 3, 12);
  tester.write(paint_mode, colors.get() + 4, 13);

  tester.write(paint_mode, colors.get() + 5, 1, 0);
  tester.write(paint_mode, colors.get() + 6, 3, 1);
  tester.write(paint_mode, colors.get() + 7, 6, 1);
  tester.write(paint_mode, colors.get() + 8, 9, 2);
  tester.write(paint_mode, colors.get() + 10, 12, 2);
  tester.write(paint_mode, colors.get() + 12, 15, 3);
  tester.write(paint_mode, colors.get() + 15, 20, 3);

  // Test some overwrites.
  tester.write(paint_mode, colors.get() + 18, 12);
  tester.write(paint_mode, colors.get() + 19, 13);

  tester.write(paint_mode, colors.get() + 20, 6, 1);
  tester.write(paint_mode, colors.get() + 21, 9, 2);
  tester.write(paint_mode, colors.get() + 23, 12, 2);
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
void TestWriteLongRandom(PaintMode paint_mode,
                         ColorMode color_mode = ColorMode()) {
  ColorRandomizer<ColorMode> colors(32768, color_mode);
  WriterTester<ColorMode, pixel_order, byte_order> tester(16, 64, color_mode);
  int pixels_total = 0;
  while (pixels_total < 32768 - 512) {
    uint32_t offset = rand() % 1024;
    uint32_t len = rand() % 512;
    if (offset + len >= 1024) len = 1024 - offset;
    tester.write(paint_mode, colors.get() + pixels_total, offset, len);
    pixels_total += len;
  }
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
void TestFillBasic(PaintMode paint_mode, ColorMode color_mode) {
  ColorRandomizer<ColorMode> colors(32, color_mode);
  WriterTester<ColorMode, pixel_order, byte_order> tester(4, 8, color_mode);
  const Color* c = colors.get();
  tester.fill(paint_mode, c[0], 0);
  tester.fill(paint_mode, c[1], 3);
  tester.fill(paint_mode, c[2], 11);
  tester.fill(paint_mode, c[3], 12);
  tester.fill(paint_mode, c[4], 13);

  tester.fill(paint_mode, c[5], 1, 0);
  tester.fill(paint_mode, c[6], 3, 1);
  tester.fill(paint_mode, c[7], 6, 1);
  tester.fill(paint_mode, c[8], 9, 2);
  tester.fill(paint_mode, c[9], 12, 2);
  tester.fill(paint_mode, c[10], 15, 3);
  tester.fill(paint_mode, c[11], 20, 3);

  // Test some overwrites.
  tester.fill(paint_mode, c[12], 12);
  tester.fill(paint_mode, c[13], 13);

  tester.fill(paint_mode, c[14], 6, 1);
  tester.fill(paint_mode, c[15], 9, 2);
  tester.fill(paint_mode, c[16], 12, 2);
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
void TestFillLongRandom(PaintMode paint_mode, ColorMode color_mode) {
  ColorRandomizer<ColorMode> colors(128, color_mode);
  WriterTester<ColorMode, pixel_order, byte_order> tester(16, 64, color_mode);
  for (int i = 0; i < 128; ++i) {
    uint32_t offset = rand() % 1024;
    uint32_t len = rand() % 512;
    if (offset + len >= 1024) len = 1024 - offset;
    tester.fill(paint_mode, colors.get()[i], offset, len);
  }
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
void TestWriter(PaintMode paint_mode, ColorMode color_mode) {
  TestWriteBasic<ColorMode, pixel_order, byte_order>(paint_mode, color_mode);
  TestWriteLongRandom<ColorMode, pixel_order, byte_order>(paint_mode,
                                                          color_mode);
}

template <typename ColorMode, ColorPixelOrder pixel_order>
void TestWriter(PaintMode paint_mode, ColorMode color_mode) {
  TestWriter<ColorMode, pixel_order, BYTE_ORDER_BIG_ENDIAN>(paint_mode,
                                                            color_mode);
  TestWriter<ColorMode, pixel_order, BYTE_ORDER_LITTLE_ENDIAN>(paint_mode,
                                                               color_mode);
}

template <typename ColorMode>
void TestWriter(PaintMode paint_mode, ColorMode color_mode = ColorMode()) {
  TestWriter<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST>(paint_mode, color_mode);
  TestWriter<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST>(paint_mode, color_mode);
}

TEST(ReplaceWriter, Grayscale4) { TestWriter<Grayscale4>(PAINT_MODE_REPLACE); }
TEST(ReplaceWriter, Grayscale8) { TestWriter<Grayscale8>(PAINT_MODE_REPLACE); }
TEST(ReplaceWriter, Argb8888) { TestWriter<Argb8888>(PAINT_MODE_REPLACE); }
TEST(ReplaceWriter, Argb6666) { TestWriter<Argb6666>(PAINT_MODE_REPLACE); }
TEST(ReplaceWriter, Argb4444) { TestWriter<Argb4444>(PAINT_MODE_REPLACE); }
TEST(ReplaceWriter, Rgb565) { TestWriter<Rgb565>(PAINT_MODE_REPLACE); }
TEST(ReplaceWriter, Alpha8) {
  TestWriter<Alpha8>(PAINT_MODE_REPLACE, color::Black);
}
TEST(ReplaceWriter, Alpha4) {
  TestWriter<Alpha4>(PAINT_MODE_REPLACE, color::Black);
}

TEST(ReplaceWriter, Rgb565WithTransparency) {
  TestWriter<Rgb565WithTransparency>(PAINT_MODE_REPLACE,
                                     Rgb565WithTransparency(12));
}

TEST(BlendWriter, Grayscale4) { TestWriter<Grayscale4>(PAINT_MODE_BLEND); }
TEST(BlendWriter, Grayscale8) { TestWriter<Grayscale8>(PAINT_MODE_BLEND); }
TEST(BlendWriter, Argb8888) { TestWriter<Argb8888>(PAINT_MODE_BLEND); }
TEST(BlendWriter, Argb6666) { TestWriter<Argb6666>(PAINT_MODE_BLEND); }
TEST(BlendWriter, Argb4444) { TestWriter<Argb4444>(PAINT_MODE_BLEND); }
TEST(BlendWriter, Rgb565) { TestWriter<Rgb565>(PAINT_MODE_BLEND); }
TEST(BlendWriter, Alpha8) {
  TestWriter<Alpha8>(PAINT_MODE_BLEND, color::Black);
}
TEST(BlendWriter, Alpha4) {
  TestWriter<Alpha4>(PAINT_MODE_BLEND, color::Black);
}

TEST(BlendWriter, Rgb565WithTransparency) {
  TestWriter<Rgb565WithTransparency>(PAINT_MODE_BLEND,
                                     Rgb565WithTransparency(12));
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
void TestFiller(PaintMode paint_mode, ColorMode color_mode = ColorMode()) {
  TestFillBasic<ColorMode, pixel_order, byte_order>(paint_mode, color_mode);
  TestFillLongRandom<ColorMode, pixel_order, byte_order>(paint_mode,
                                                         color_mode);
}

template <typename ColorMode, ColorPixelOrder pixel_order>
void TestFiller(PaintMode paint_mode, ColorMode color_mode = ColorMode()) {
  TestFiller<ColorMode, pixel_order, BYTE_ORDER_BIG_ENDIAN>(paint_mode,
                                                            color_mode);
  TestFiller<ColorMode, pixel_order, BYTE_ORDER_LITTLE_ENDIAN>(paint_mode,
                                                               color_mode);
}

template <typename ColorMode>
void TestFiller(PaintMode paint_mode, ColorMode color_mode = ColorMode()) {
  TestFiller<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST>(paint_mode, color_mode);
  TestFiller<ColorMode, COLOR_PIXEL_ORDER_LSB_FIRST>(paint_mode, color_mode);
}

TEST(ReplaceFiller, Grayscale4) { TestFiller<Grayscale4>(PAINT_MODE_REPLACE); }
TEST(ReplaceFiller, Grayscale8) { TestFiller<Grayscale8>(PAINT_MODE_REPLACE); }
TEST(ReplaceFiller, Argb8888) { TestFiller<Argb8888>(PAINT_MODE_REPLACE); }
TEST(ReplaceFiller, Argb6666) { TestFiller<Argb6666>(PAINT_MODE_REPLACE); }
TEST(ReplaceFiller, Argb4444) { TestFiller<Argb4444>(PAINT_MODE_REPLACE); }
TEST(ReplaceFiller, Rgb565) { TestFiller<Rgb565>(PAINT_MODE_REPLACE); }
TEST(ReplaceFiller, Alpha8) {
  TestFiller<Alpha8>(PAINT_MODE_REPLACE, color::Black);
}
TEST(ReplaceFiller, Alpha4) {
  TestFiller<Alpha4>(PAINT_MODE_REPLACE, color::Black);
}

TEST(ReplaceFiller, Rgb565WithTransparency) {
  TestWriter<Rgb565WithTransparency>(PAINT_MODE_REPLACE,
                                     Rgb565WithTransparency(12));
}

TEST(BlendFiller, Grayscale4) { TestFiller<Grayscale4>(PAINT_MODE_BLEND); }
TEST(BlendFiller, Grayscale8) { TestFiller<Grayscale8>(PAINT_MODE_BLEND); }
TEST(BlendFiller, Argb8888) { TestFiller<Argb8888>(PAINT_MODE_BLEND); }
TEST(BlendFiller, Argb6666) { TestFiller<Argb6666>(PAINT_MODE_BLEND); }
TEST(BlendFiller, Argb4444) { TestFiller<Argb4444>(PAINT_MODE_BLEND); }
TEST(BlendFiller, Rgb565) { TestFiller<Rgb565>(PAINT_MODE_BLEND); }
TEST(BlendFiller, Alpha8) {
  TestFiller<Alpha8>(PAINT_MODE_BLEND, color::Black);
}
TEST(BlendFiller, Alpha4) {
  TestFiller<Alpha4>(PAINT_MODE_BLEND, color::Black);
}

TEST(BlendFiller, Rgb565WithTransparency) {
  TestWriter<Rgb565WithTransparency>(PAINT_MODE_BLEND,
                                     Rgb565WithTransparency(12));
}

void TestAddressWindowAdvance(uint16_t x0, uint16_t y0, uint16_t x1,
                              uint16_t y1, int16_t raw_width,
                              int16_t raw_height, Orientation orientation,
                              int max_skip) {
  internal::AddressWindow tested;
  internal::AddressWindow reference;
  tested.setAddress(x0, y0, x1, y1, raw_width, raw_height, orientation);
  reference.setAddress(x0, y0, x1, y1, raw_width, raw_height, orientation);
  std::uniform_int_distribution<> distribution(0, max_skip);
  EXPECT_EQ(reference.offset(), tested.offset());
  uint32_t pixels_left = (x1 - x0 + 1) * (y1 - y0 + 1);
  while (pixels_left > 0) {
    uint32_t next = distribution(generator);
    if (next > pixels_left) {
      next = pixels_left;
    }
    for (uint32_t i = 0; i < next; i++) {
      reference.advance();
    }
    tested.advance(next);
    EXPECT_EQ(reference.offset(), tested.offset());
    pixels_left -= next;
  }
}

void TestAddressWindowAdvance(uint16_t x0, uint16_t y0, uint16_t x1,
                              uint16_t y1, int16_t raw_width,
                              int16_t raw_height, int max_skip) {
  Orientation o = Orientation::RightDown();
  for (int i = 0; i < 4; ++i) {
    TestAddressWindowAdvance(x0, y0, x1, y1, raw_width, raw_height, o,
                             max_skip);
    o = o.swapXY();
    TestAddressWindowAdvance(x0, y0, x1, y1, raw_width, raw_height, o,
                             max_skip);
    o = o.swapXY().rotateRight();
  }
}

TEST(AddressWindow, Advance1) {
  TestAddressWindowAdvance(5, 6, 13, 63, 80, 90, 1);
}

TEST(AddressWindow, Advance10) {
  TestAddressWindowAdvance(1, 3, 53, 43, 80, 90, 10);
}

TEST(AddressWindow, Advance200) {
  TestAddressWindowAdvance(1, 3, 53, 43, 80, 90, 200);
}

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
const Raster<const uint8_t*, ColorMode, pixel_order, byte_order> RasterOf(
    const OffscreenDevice<ColorMode, pixel_order, byte_order>& offscreen) {
  return offscreen.raster();
}

class OffscreenTest
    : public testing::TestWithParam<std::tuple<PaintMode, Orientation>> {};

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class OffscreenDeviceForTest
    : public OffscreenDevice<ColorMode, pixel_order, byte_order,
                             pixels_per_byte, storage_type> {
 public:
  typedef OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                          storage_type>
      Base;

  OffscreenDeviceForTest(int16_t width, int16_t height, Color bg)
      : Base(width, height,
             new uint8_t[(ColorMode::bits_per_pixel * width * height + 7) / 8],
             ColorMode()) {
    Base::fillRect(0, 0, width - 1, height - 1, bg);
  }

  ~OffscreenDeviceForTest() { delete[] Base::buffer(); }
};

TEST_P(OffscreenTest, FillRects) {
  TestFillRects<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, FillHLines) {
  TestFillHLines<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, FillVLines) {
  TestFillVLines<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, FillDegeneratePixels) {
  TestFillDegeneratePixels<OffscreenDeviceForTest<Argb4444>,
                           FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                    std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, FillPixels) {
  TestFillPixels<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WriteRects) {
  TestWriteRects<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WriteHLines) {
  TestWriteHLines<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WriteVLines) {
  TestWriteVLines<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WriteDegeneratePixels) {
  TestWriteDegeneratePixels<OffscreenDeviceForTest<Argb4444>,
                            FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                     std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WritePixels) {
  TestWritePixels<OffscreenDeviceForTest<Argb4444>, FakeOffscreen<Argb4444>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WritePixelsStress) {
  TestWritePixelsStress<OffscreenDeviceForTest<Argb4444>,
                        FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                 std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WritePixelsSnake) {
  TestWritePixelsSnake<OffscreenDeviceForTest<Argb4444>,
                       FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WriteRectWindowSimple) {
  TestWriteRectWindowSimple<OffscreenDeviceForTest<Argb4444>,
                            FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                     std::get<1>(GetParam()));
}

TEST_P(OffscreenTest, WriteRectWindowStress) {
  TestWriteRectWindowStress<OffscreenDeviceForTest<Argb4444>,
                            FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                     std::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    OffscreenTests, OffscreenTest,
    testing::Combine(
        testing::Values(PAINT_MODE_REPLACE, PAINT_MODE_BLEND),
        testing::Values(Orientation::RightDown(), Orientation::DownRight(),
                        Orientation::LeftDown(), Orientation::DownLeft(),
                        Orientation::RightUp(), Orientation::UpRight(),
                        Orientation::LeftUp(), Orientation::UpLeft())));

// Now, let's also test some basic functionality.

TEST_F(OffscreenTest, OffsetedDrawing) {
  Offscreen<Monochrome> offscreen(Box(-6, -5, 4, 3), color::Black,
                                  WhiteOnBlack());
  EXPECT_THAT(offscreen.raster(), MatchesContent(WhiteOnBlack(), 11, 9,
                                                 "           "
                                                 "           "
                                                 "           "
                                                 "           "
                                                 "           "
                                                 "           "
                                                 "           "
                                                 "           "
                                                 "           "));

  DrawingContext dc(offscreen);
  EXPECT_FALSE(dc.transform().is_rescaled());
  EXPECT_FALSE(dc.transform().is_translated());
  EXPECT_EQ(dc.bounds(), Box(-6, -5, 4, 3));
  dc.draw(SolidRect(-4, -3, 2, 1, color::White));
  EXPECT_THAT(offscreen.raster(), MatchesContent(WhiteOnBlack(), 11, 9,
                                                 "           "
                                                 "           "
                                                 "  *******  "
                                                 "  *******  "
                                                 "  *******  "
                                                 "  *******  "
                                                 "  *******  "
                                                 "           "
                                                 "           "));
  dc.setClipBox(-100, -100, 1, 0);
  EXPECT_EQ(dc.getClipBox(), Box(-6, -5, 1, 0));
  dc.clear();
  EXPECT_THAT(offscreen.raster(), MatchesContent(WhiteOnBlack(), 11, 9,
                                                 "           "
                                                 "           "
                                                 "        *  "
                                                 "        *  "
                                                 "        *  "
                                                 "        *  "
                                                 "  *******  "
                                                 "           "
                                                 "           "));
}

TEST_F(OffscreenTest, DrawingOffscreen) {
  Offscreen<Grayscale4> offscreen(Box(-6, -5, 4, 3), color::Black);
  DrawingContext dc(offscreen);
  dc.draw(SolidRect(-4, -3, 2, 1, Color(0xFF444444)));
  dc.draw(SolidRect(-3, -2, 1, 0, Color(0xFF666666)));
  FakeOffscreen<Argb4444> test_screen(13, 9, color::White);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(offscreen, 5, 4);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 13, 9,
                                          "          ***"
                                          " 4444444  ***"
                                          " 4666664  ***"
                                          " 4666664  ***"
                                          " 4666664  ***"
                                          " 4444444  ***"
                                          "          ***"
                                          "          ***"
                                          "*************"));
}

TEST_F(OffscreenTest, DrawingOffscreenStreamable) {
  Offscreen<Grayscale4> offscreen(Box(-6, -5, 4, 3), color::Black);
  DrawingContext dc(offscreen);
  dc.draw(SolidRect(-4, -3, 2, 1, Color(0xFF444444)));
  dc.draw(SolidRect(-3, -2, 1, 0, Color(0xFF666666)));
  FakeOffscreen<Argb4444> test_screen(13, 9, color::White);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(ForcedStreamable(&offscreen), 5, 4);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 13, 9,
                                          "          ***"
                                          " 4444444  ***"
                                          " 4666664  ***"
                                          " 4666664  ***"
                                          " 4666664  ***"
                                          " 4444444  ***"
                                          "          ***"
                                          "          ***"
                                          "*************"));
}

TEST_F(OffscreenTest, DrawingOffscreenRasterizable) {
  Offscreen<Grayscale4> offscreen(Box(-6, -5, 4, 3), color::Black);
  DrawingContext dc(offscreen);
  dc.draw(SolidRect(-4, -3, 2, 1, Color(0xFF444444)));
  dc.draw(SolidRect(-3, -2, 1, 0, Color(0xFF666666)));
  FakeOffscreen<Argb4444> test_screen(13, 9, color::White);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(ForcedRasterizable(&offscreen), 5, 4);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 13, 9,
                                          "          ***"
                                          " 4444444  ***"
                                          " 4666664  ***"
                                          " 4666664  ***"
                                          " 4666664  ***"
                                          " 4444444  ***"
                                          "          ***"
                                          "          ***"
                                          "*************"));
}

}  // namespace roo_display

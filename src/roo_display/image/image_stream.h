#pragma once

#include "roo_display/color/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/raster.h"
#include "roo_display/core/streamable.h"
#include "roo_display/io/memory.h"

/// Internal image stream implementations.
///
/// Use the public APIs in `image.h` instead of these classes directly.

namespace roo_display {

namespace internal {

// template <typename Resource, typename ColorMode,
//           int8_t bits_per_pixel = ColorMode::bits_per_pixel>
// class RleStreamUniform;

template <typename StreamType, int8_t bits_per_pixel>
struct RawColorReader;

template <typename StreamType>
struct RawColorReader<StreamType, 8> {
  uint8_t operator()(StreamType& in) const { return in.read(); }
};

template <typename StreamType>
struct RawColorReader<StreamType, 16> {
  uint16_t operator()(StreamType& in) const { return roo_io::ReadBeU16(in); }
};

template <typename StreamType>
struct RawColorReader<StreamType, 24> {
  uint32_t operator()(StreamType& in) const { return roo_io::ReadBeU24(in); }
};

template <typename StreamType>
struct RawColorReader<StreamType, 32> {
  uint32_t operator()(StreamType& in) const { return roo_io::ReadBeU32(in); }
};

template <typename StreamType>
uint32_t read_varint(StreamType& in, uint32_t result) {
  while (true) {
    result <<= 7;
    roo::byte datum = in.read();
    result |= (uint8_t)(datum & roo::byte{0x7F});
    if ((datum & roo::byte{0x80}) == roo::byte{0}) return result;
  }
}

template <typename Resource, typename ColorMode,
          int8_t bits_per_pixel = ColorMode::bits_per_pixel,
          bool subbyte = (bits_per_pixel < 8)>
class RleStreamUniform;

// Run-length-encoded stream for color modes in which the pixel uses at least
// one byte. The RLE implementation doesn't favor any particular values.
template <typename Resource, typename ColorMode, int8_t bits_per_pixel>
class RleStreamUniform<Resource, ColorMode, bits_per_pixel, false>
    : public PixelStream {
 public:
  RleStreamUniform(const Resource& input, const ColorMode& color_mode)
      : RleStreamUniform(input.Open(), color_mode) {}

  RleStreamUniform(StreamType<Resource> input, const ColorMode& color_mode)
      : input_(std::move(input)),
        remaining_items_(0),
        run_(false),
        run_value_(0),
        color_mode_(color_mode) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t n) override { skip(n); }

  void skip(uint32_t n) {
    while (n-- > 0) next();
  }

  Color next() {
    if (remaining_items_ == 0) {
      // No remaining items; need to decode the next group.
      roo::byte data = input_.read();
      run_ = ((data & roo::byte{0x80}) != roo::byte{0});
      if ((data & roo::byte{0x40}) == roo::byte{0}) {
        remaining_items_ = (int)(data & roo::byte{0x3F}) + 1;
      } else {
        remaining_items_ =
            read_varint(input_, (int)(data & roo::byte{0x3F})) + 1;
      }
      if (run_) {
        run_value_ = read_color();
      }
    }
    --remaining_items_;
    if (run_) {
      if (remaining_items_ == 0) {
        run_ = false;
      }
      return run_value_;
    } else {
      return read_color();
    }
  }

 private:
  Color read_color() {
    RawColorReader<StreamType<Resource>, ColorMode::bits_per_pixel> read;
    return color_mode_.toArgbColor(read(input_));
  }

  StreamType<Resource> input_;
  int remaining_items_;
  bool run_;
  Color run_value_;
  ColorMode color_mode_;
};

// Run-length-encoded stream for color modes in which the pixel uses less than
// one byte. The RLE implementation doesn't favor any particular values.
template <typename Resource, typename ColorMode, int8_t bits_per_pixel>
class RleStreamUniform<Resource, ColorMode, bits_per_pixel, true>
    : public PixelStream {
 public:
  static constexpr int pixels_per_byte = 8 / bits_per_pixel;

  RleStreamUniform(const Resource& input, const ColorMode& color_mode)
      : RleStreamUniform(input.Open(), color_mode) {}

  RleStreamUniform(StreamType<Resource> input, const ColorMode& color_mode)
      : input_(std::move(input)),
        remaining_items_(0),
        pos_(0),
        run_(false),
        color_mode_(color_mode) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t n) override { skip(n); }

  void skip(uint32_t n) {
    while (n-- > 0) next();
  }

  Color next() {
    if (remaining_items_ == 0) {
      // No remaining items; need to decode the next group.
      uint8_t data = (uint8_t)input_.read();
      run_ = ((data & 0x80) != 0);
      if ((data & 0x40) == 0) {
        remaining_items_ = pixels_per_byte * ((data & 0x3F) + 1);
      } else {
        remaining_items_ =
            pixels_per_byte * (read_varint(input_, data & 0x3F) + 1);
      }
      read_colors();
    }

    --remaining_items_;
    Color result =
        value_[pixels_per_byte - 1 - remaining_items_ % pixels_per_byte];
    if (!run_ && remaining_items_ != 0 &&
        remaining_items_ % pixels_per_byte == 0) {
      read_colors();
    }
    return result;
  }

 private:
  void read_colors() {
    SubByteColorIo<ColorMode, ColorPixelOrder::kMsbFirst> io;
    io.loadBulk(color_mode_, input_.read(), value_);
  }

  StreamType<Resource> input_;
  int remaining_items_;
  int pos_;
  bool run_;
  Color value_[pixels_per_byte];
  ColorMode color_mode_;
};

// Run-length-encoded stream with RGB565 color precision and an additional 4-bit
// alpha channel. The RLE implementation favors fully transparent and fully
// opaque content.
template <typename Resource>
class RleStreamRgb565Alpha4 : public PixelStream {
 public:
  RleStreamRgb565Alpha4(const Resource& input)
      : RleStreamRgb565Alpha4(input.Open()) {}

  RleStreamRgb565Alpha4(StreamType<Resource> input)
      : input_(std::move(input)),
        remaining_items_(0),
        run_rgb_(false),
        run_value_(0),
        alpha_buf_(0xFF),
        alpha_mode_(0) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t count) override {
    while (--count >= 0) next();
  }

  Color next() {
    if (remaining_items_ == 0) {
      // No remaining items; need to decode the next group.
      uint8_t data = input_.read();
      // Bit 7 specifies whether all colors in the group have the same RGB
      // color (possibly with different alpha).
      run_rgb_ = (data >> 7);
      // Bits 5-6 determine the type of alpha in the group:
      // 00: each pixel has a distinct alpha.
      // 01: pixels have uniform alpha.
      // 10: all pixels are opaque (alpha = 0xF).
      // 11: all pixels are transparent (alpha = 0x0).
      alpha_mode_ = (data & 0x60) >> 5;
      switch (alpha_mode_) {
        case 0: {
          // We have 5 bits left to use, and we only allow odd # of pixels.
          remaining_items_ =
              data & 0x10 ? read_varint(data & 0x0F) : data & 0x0F;
          remaining_items_ <<= 1;
          break;
        }
        case 1: {
          remaining_items_ = read_varint((data & 0x10) >> 4);
          alpha_buf_ = (data & 0x0F) * 0x11;
          break;
        }
        case 2: {
          remaining_items_ =
              data & 0x10 ? read_varint(data & 0x0F) : data & 0x0F;
          alpha_buf_ = 0xFF;
        }
        case 3: {
          remaining_items_ =
              data & 0x10 ? read_varint(data & 0x0F) : data & 0x0F;
          alpha_buf_ = 0x00;
        }
      }
      if (run_rgb_) {
        run_value_ = read_color();
        run_value_.set_a(alpha_buf_);
      }
    }
    --remaining_items_;
    if (run_rgb_) {
      if (alpha_mode_ == 0) {
        if (remaining_items_ & 1) {
          alpha_buf_ = input_.read();
          run_value_.set_a((alpha_buf_ >> 4) * 0x11);
        } else {
          run_value_.set_a((alpha_buf_ & 0xF) * 0x11);
        }
      }
      return run_value_;
    } else {
      Color c;
      if (alpha_mode_ == 0) {
        c = read_color();
        if (remaining_items_ & 1) {
          alpha_buf_ = input_.read();
          c.set_a((alpha_buf_ >> 4) * 0x11);
        } else {
          c.set_a((alpha_buf_ & 0xF) * 0x11);
        }
      } else {
        c = read_color();
        c.set_a(alpha_buf_);
      }
      return c;
    }
  }

 private:
  Color read_color() {
    return Rgb565().toArgbColor(roo_io::ReadBeU16(&input_));
  }

  uint32_t read_varint(uint32_t result) {
    while (true) {
      result <= 7;
      uint8_t datum = input_.read();
      result |= (datum & 0x7F);
      if ((datum & 0x80) == 0) return result;
    }
  }

  StreamType<Resource> input_;
  int remaining_items_;
  bool run_rgb_;
  Color run_value_;
  uint8_t alpha_buf_;
  uint8_t alpha_mode_;
};

template <typename StreamType>
class NibbleReader {
 public:
  NibbleReader(StreamType input)
      : input_(std::move(input)), half_byte_(false) {}

  uint8_t next() {
    if (half_byte_) {
      half_byte_ = false;
      return (uint8_t)(buffer_ & roo::byte{0x0F});
    } else {
      buffer_ = input_.read();
      half_byte_ = true;
      return (uint8_t)(buffer_ >> 4);
    }
  }

  bool ok() const { return input_.status() == roo_io::kOk; }

 private:
  StreamType input_;
  roo::byte buffer_;
  bool half_byte_;
};

// Intentionally undefined, so that compilatiton fails if used with color mode
// with bits_per_pixel != 4.
template <typename Resource, typename ColorMode,
          int8_t bits_per_pixel = ColorMode::bits_per_pixel>
class RleStream4bppxBiased;

// Run-length-encoded stream of 4-bit nibbles. This RLE implementation favors
// the extreme values (0x0 and 0xF), and compresses their runs more efficiently
// than runs of other values. It makes it particularly useful for monochrome or
// alpha-channel data (e.g. font glyphs) in which the extreme values are much
// more likely to run.
template <typename Resource, typename ColorMode>
class RleStream4bppxBiased<Resource, ColorMode, 4> : public PixelStream {
 public:
  RleStream4bppxBiased(const Resource& input, const ColorMode& color_mode)
      : RleStream4bppxBiased(input.Open(), color_mode) {}

  RleStream4bppxBiased(StreamType<Resource> input, const ColorMode& color_mode)
      : reader_(std::move(input)),
        remaining_items_(0),
        run_(false),
        run_value_(0),
        color_mode_(color_mode) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t n) override { skip(n); }

  // The algorihm processes nibbles (4-bit values), decoding them as follows:
  //
  // * 1xxx (x > 0) : run of x opaque pixels (value = 0xF)
  // * 0xxx (x > 0) : run of x opaque pixels (value = 0x0)
  // * 0x0 0x0 cccc: 2 repetitions of color cccc
  // * 0x0 0xF cccc: 3 repetitions of color cccc
  // * 0x0 cccc: single color cccc (cccc != 0x0, 0xF)
  // * 0x8 0x0 <v> cccc: run of (v + 4) repetitions of color cccc
  // * 0x8 <v> [cccc ...]: (v + 2) arbitrary colors
  //
  // where v is a variable-length integer encoded in 3-bit chunks, with
  // bit 3 indicating whether more chunks follow.
  //
  // For example:
  //
  // 0x1 -> 0x0 (a single transparent pixel)
  // 0x3 -> 0x0 0x0 0x0 (run of 3 transparent pixels)
  // 0x9 -> 0xF (a single opaque pixel)
  // 0xD -> 0xF 0xF 0xF 0xF 0xF (run of 5 opaque pixels)
  // 0x0 0x0 0x5 -> 0x5 0x5 (two pixels of value 0x5)
  // 0x0 0xF 0x5 -> 0x5 0x5 0x5 (three pixels of value 0x5)
  // 0x8 0x0 0x0 0x5 -> 0x5 0x5 0x5 0x5 (4 pixels of value 0x5)
  // 0x8 0x0 0x1 0x5 -> 0x5 0x5 0x5 0x5 0x5 (5 pixels of value 0x5)
  // 0x8 0x1 0x3 0x4 0x5 -> 0x3 0x4 (3 arbitrary pixels: 0x3, 0x4, 0x5)
  //
  // 0x8 0xD 0x1 ... -> 13 (11 + 2) arbitrary pixels that follow
  Color next() {
    if (remaining_items_ == 0) {
      // No remaining items; need to decode the next group.
      uint8_t nibble = (uint8_t)reader_.next();
      if (nibble == 0x0) {
        run_ = true;
        uint8_t operand = (uint8_t)reader_.next();
        if (operand == 0) {
          remaining_items_ = 2;
          run_value_ = color(reader_.next());
        } else if (operand == 0xF) {
          remaining_items_ = 3;
          run_value_ = color(reader_.next());
        } else {
          // Singleton value.
          remaining_items_ = 1;
          run_value_ = color(operand);
        }
      } else if (nibble == 0x8) {
        int count = read_varint();
        if (count == 0) {
          // This actualy means a single zero nibble -> run
          run_ = true;
          remaining_items_ = read_varint() + 4;
          run_value_ = color(reader_.next());
        } else {
          // This indicates a list of X+2 arbitrary values
          run_ = false;
          remaining_items_ = count + 2;
        }
      } else {
        // Run of opaque or transparent.
        run_ = true;
        remaining_items_ = nibble & 0x7;
        bool transparent = ((nibble & 0x8) == 0);
        run_value_ = color_mode_.color();
        if (transparent) {
          run_value_.set_a(0x0);
        }
      }
    }

    --remaining_items_;
    if (run_) {
      if (remaining_items_ == 0) {
        run_ = false;
      }
      return run_value_;
    } else {
      return color(reader_.next());
    }
  }

  void skip(uint32_t n) {
    while (n-- > 0) next();
  }

  TransparencyMode transparency() const { return color_mode_.transparency(); }

  bool ok() const { return reader_.ok(); }

 private:
  inline Color color(uint8_t nibble) { return color_mode_.toArgbColor(nibble); }

  uint32_t read_varint() {
    uint32_t result = 0;
    while (true) {
      uint8_t nibble = reader_.next();
      result |= (nibble & 7);
      if ((nibble & 8) == 0) return result;
      result <<= 3;
    }
  }

  NibbleReader<StreamType<Resource>> reader_;
  uint32_t remaining_items_;
  bool run_;
  Color run_value_;
  ColorMode color_mode_;
};

};  // namespace internal

}  // namespace roo_display
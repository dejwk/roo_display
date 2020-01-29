#pragma once

#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/raster.h"
#include "roo_display/core/streamable.h"
#include "roo_display/io/memory.h"

// Image streams used by supported image types. Don't use these directly;
// use "image.h" instead.

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
  uint16_t operator()(StreamType& in) const { return read_uint16_be(&in); }
};

template <typename StreamType>
struct RawColorReader<StreamType, 24> {
  uint32_t operator()(StreamType& in) const { return read_uint24_be(&in); }
};

template <typename StreamType>
struct RawColorReader<StreamType, 32> {
  uint32_t operator()(StreamType& in) const { return read_uint32_be(&in); }
};

template <typename StreamType>
uint32_t read_varint(StreamType& in, uint32_t result) {
  while (true) {
    result <<= 7;
    uint8_t datum = in.read();
    result |= (datum & 0x7F);
    if ((datum & 0x80) == 0) return result;
  }
}

// Run-length-encoded stream for color modes in which the pixel uses at least
// one byte. The RLE implementation doesn't favor any particular values.
template <typename Resource, typename ColorMode,
          int8_t bits_per_pixel = ColorMode::bits_per_pixel>
class RleStreamUniform {
 public:
  RleStreamUniform(const Resource& input, const ColorMode& mode)
      : RleStreamUniform(input.Open(), mode) {}

  RleStreamUniform(StreamType<Resource> input, const ColorMode& mode)
      : input_(std::move(input)),
        remaining_items_(0),
        run_(false),
        run_value_(0),
        mode_(mode) {}

  Color next() {
    if (remaining_items_ == 0) {
      // No remaining items; need to decode the next group.
      uint8_t data = input_.read();
      run_ = ((data & 0x80) != 0);
      if ((data & 0x40) == 0) {
        remaining_items_ = (data & 0x3F) + 1;
      } else {
        remaining_items_ = read_varint(input_, data & 0x3F) + 1;
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

  TransparencyMode transparency() const { return mode_.transparency(); }

  void skip(int count) {
    while (--count >= 0) next();
  }

 private:
  Color read_color() {
    RawColorReader<StreamType<Resource>, bits_per_pixel> read;
    return mode_.toArgbColor(read(input_));
  }

  StreamType<Resource> input_;
  int remaining_items_;
  bool run_;
  Color run_value_;
  ColorMode mode_;
};

// Run-length-encoded stream with RGB565 color precision and an additional 4-bit
// alpha channel. The RLE implementation favors fully transparent and fully
// opaque content.
template <typename Resource>
class RleStreamRgb565Alpha4 {
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

  TransparencyMode transparency() const { return TRANSPARENCY_GRADUAL; }

  void skip(int count) {
    while (--count >= 0) next();
  }

 private:
  Color read_color() { return Rgb565().toArgbColor(read_uint16_be(&input_)); }
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
      return buffer_ & 0x0F;
    } else {
      buffer_ = input_.read();
      half_byte_ = true;
      return buffer_ >> 4;
    }
  }

 private:
  StreamType input_;
  uint8_t buffer_;
  bool half_byte_;
};

// Intentionally undefined, so that compilatiton fails if used with color mode
// with bits_per_pixel != 4.
template <typename Resource, typename ColorMode,
          int8_t bits_per_pixel = ColorMode::bits_per_pixel>
class RleStream4bppxPolarized;

// Run-length-encoded stream of 4-bit nibbles. This RLE implementation favors
// the extreme values (0x0 and 0xF), and compresses their runs more efficiently
// than runs of other values. It makes it particularly useful for monochrome or
// alpha-channel data (e.g. font glyphs) in which the extreme values are much
// more likely to run.
template <typename Resource, typename ColorMode>
class RleStream4bppxPolarized<Resource, ColorMode, 4> {
 public:
  RleStream4bppxPolarized(const Resource& input, const ColorMode& mode)
      : RleStream4bppxPolarized(input.Open(), mode) {}

  RleStream4bppxPolarized(StreamType<Resource> input, const ColorMode& mode)
      : reader_(std::move(input)),
        remaining_items_(0),
        run_(false),
        run_value_(0),
        mode_(mode) {}

  Color next() {
    if (remaining_items_ == 0) {
      // No remaining items; need to decode the next group.
      uint8_t nibble = reader_.next();
      if (nibble == 0x0) {
        run_ = true;
        uint8_t operand = reader_.next();
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
        run_value_ = mode_.color();
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

  void skip(int32_t count) {
    while (--count >= 0) next();
  }

  TransparencyMode transparency() const { return mode_.transparency(); }

 private:
  inline Color color(uint8_t nibble) { return mode_.toArgbColor(nibble); }

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
  ColorMode mode_;
};

template <typename Resource>
class Rgb565Alpha4RleStream {
 public:
  Rgb565Alpha4RleStream(const Resource& input)
      : Rgb565Alpha4RleStream(input, Argb8888()) {}

  Rgb565Alpha4RleStream(const Resource& input, const Argb8888& ignored)
      : color_stream_(skip4(input), Rgb565()),
        alpha_stream_(skip_color(input), color::Black) {}

  Color next() {
    Color c = color_stream_.next();
    Color alpha = alpha_stream_.next();
    c.set_a(alpha.a());
    return c;
  }

  void skip(int32_t count) {
    while (--count >= 0) next();
  }

  TransparencyMode transparency() const { return TRANSPARENCY_GRADUAL; }

 private:
  static inline StreamType<Resource> skip4(const Resource& input) {
    StreamType<Resource> stream = input.Open();
    stream.skip(4);
    return stream;
  };

  static inline StreamType<Resource> skip_color(const Resource& input) {
    StreamType<Resource> stream = input.Open();
    stream.skip(read_uint32_be(&stream));
    return stream;
  };

  RleStreamUniform<Resource, Rgb565> color_stream_;
  RleStream4bppxPolarized<Resource, Alpha4> alpha_stream_;
};

};  // namespace internal

}  // namespace roo_display
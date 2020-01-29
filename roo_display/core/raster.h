#pragma once

#include "roo_display/core/color.h"
#include "roo_display/core/color_subpixel.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/io/memory.h"

namespace roo_display {

// Helper to iterate over pixels read from some underlying resource.
// Default implementation for color modes with multiple pixels per byte.
// The resource is interpreted by this reader simply as a non-compressed
// stream of bytes, containing consecutive pixels. Any semantics of lines and
// columns of pixels must be handled by the caller.
template <typename Resource, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order,
          int pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte>
class PixelStream {
 public:
  PixelStream(const Resource& resource, const ColorMode& color_mode)
      : PixelStream(resource.Open(), color_mode) {}

  PixelStream(StreamType<Resource> stream, const ColorMode& color_mode)
      : stream_(std::move(stream)), pixel_index_(0), color_mode_(color_mode) {
    fetch();
  }

  // Advances the iterator to the next pixel in the buffer.
  Color next() {
    Color color = cache_[pixel_index_];
    if (++pixel_index_ == ColorTraits<ColorMode>::pixels_per_byte) {
      pixel_index_ = 0;
      fetch();
    }
    return color;
  }

  void skip(uint32_t count) {
    uint32_t new_pixel_index = count + pixel_index_;
    uint32_t bytes_to_skip =
        new_pixel_index / ColorTraits<ColorMode>::pixels_per_byte;
    if (bytes_to_skip > 0) {
      stream_.advance(bytes_to_skip - 1);
      fetch();
    }
    pixel_index_ = new_pixel_index % ColorTraits<ColorMode>::pixels_per_byte;
  }

  TransparencyMode transparency() const { return color_mode_.transparency(); }

  const ColorMode& color_mode() const { return color_mode_; }

 private:
  void fetch() {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    subpixel.ReadSubPixelColorBulk(color_mode_, stream_.read(), cache_);
  }

  StreamType<Resource> stream_;
  uint8_t pixel_index_;
  const ColorMode& color_mode_;
  Color cache_[ColorTraits<ColorMode>::pixels_per_byte];
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order,
          int bytes_per_pixel = ColorTraits<ColorMode>::bytes_per_pixel>
struct ColorReader;

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 1> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    return color_mode.toArgbColor(in.read());
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 2> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    StreamReader16<byte_order> read;
    return color_mode.toArgbColor(read(&in));
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 3> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    StreamReader24<byte_order> read;
    return color_mode.toArgbColor(read(&in));
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 4> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    StreamReader32<byte_order> read;
    return color_mode.toArgbColor(read(&in));
  }
};

// Specialization for color multibyte-pixel color modes (e.g. RGB565).
template <typename Resource, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
class PixelStream<Resource, ColorMode, pixel_order, byte_order, 1> {
 public:
  PixelStream(const Resource& resource, const ColorMode& color_mode)
      : PixelStream(resource.Open(), color_mode) {}

  PixelStream(StreamType<Resource> stream, const ColorMode& color_mode)
      : stream_(std::move(stream)), color_mode_(color_mode) {}

  // Advances the iterator to the next pixel in the buffer.
  Color next() {
    ColorReader<StreamType<Resource>, ColorMode, pixel_order, byte_order> read;
    return read(stream_, color_mode_);
  }

  void skip(uint32_t count) {
    stream_.advance(count * ColorMode::bits_per_pixel / 8);
  }

  TransparencyMode transparency() const { return color_mode_.transparency(); }

  const ColorMode& color_mode() const { return color_mode_; }

 private:
  StreamType<Resource> stream_;
  const ColorMode& color_mode_;
};

// Allows drawing rectangular pixmaps (e.g. read from DRAM, PROGMEM, or file),
// using various color modes.
template <typename Resource, typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using Raster =
    SimpleStreamable<Resource, ColorMode,
                     PixelStream<Resource, ColorMode, pixel_order, byte_order>>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterArgb8888 =
    Raster<Resource, Argb8888, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterArgb6666 =
    Raster<Resource, Argb6666, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterArgb4444 =
    Raster<Resource, Argb4444, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterRgb565 =
    Raster<Resource, Rgb565, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterRgb565WithTransparency =
    Raster<Resource, Rgb565WithTransparency, COLOR_PIXEL_ORDER_MSB_FIRST,
           byte_order>;

template <typename Resource>
using RasterGrayscale8 =
    Raster<Resource, Grayscale8, COLOR_PIXEL_ORDER_MSB_FIRST,
           BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource>
using RasterAlpha8 = Raster<Resource, Alpha8, COLOR_PIXEL_ORDER_MSB_FIRST,
                            BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using RasterGrayscale4 =
    Raster<Resource, Grayscale4, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using RasterAlpha4 =
    Raster<Resource, Alpha4, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using RasterMonochrome =
    Raster<Resource, Monochrome, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

}  // namespace roo_display
#pragma once

#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/internal/color_subpixel.h"
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
class RasterPixelStream : public PixelStream {
 public:
  // RasterPixelStream(const Resource& resource, const ColorMode& color_mode)
  //     : RasterPixelStream(resource.Open(), color_mode) {}

  RasterPixelStream(StreamType<Resource> stream, const ColorMode& color_mode)
      : stream_(std::move(stream)), pixel_index_(0), color_mode_(color_mode) {
    fetch();
  }

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t count) override { skip(count); }

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
class RasterPixelStream<Resource, ColorMode, pixel_order, byte_order, 1>
    : public PixelStream {
 public:
  // RasterPixelStream(const Resource& resource, const ColorMode& color_mode)
  //     : PixelStream(resource.Open(), color_mode) {}

  RasterPixelStream(StreamType<Resource> stream, const ColorMode& color_mode)
      : stream_(std::move(stream)), color_mode_(color_mode) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t count) override { skip(count); }

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

namespace internal {

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
struct Reader {
  storage_type operator()(const uint8_t* p, uint32_t offset) const {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    const uint8_t* target = p + offset / pixels_per_byte;
    return subpixel.ReadSubPixelColor(*target, pixel_index);
  }
};

template <typename RawType, int bits_per_pixel, ByteOrder byte_order>
struct ConstRawReader {
  RawType operator()(const uint8_t* ptr, uint32_t offset) const {
    return byte_order::toh<RawType, byte_order>(((const RawType*)ptr)[offset]);
  }
};

template <>
struct ConstRawReader<uint32_t, 24, BYTE_ORDER_BIG_ENDIAN> {
  uint32_t operator()(const uint8_t* ptr, uint32_t offset) const {
    ptr += 3 * offset;
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
  }
};

template <>
struct ConstRawReader<uint32_t, 24, BYTE_ORDER_LITTLE_ENDIAN> {
  uint32_t operator()(const uint8_t* ptr, uint32_t offset) const {
    ptr += 3 * offset;
    return ptr[2] << 16 | ptr[1] << 8 | ptr[0];
  }
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
struct Reader<ColorMode, pixel_order, byte_order, 1, storage_type> {
  storage_type operator()(const uint8_t* p, uint32_t offset) const {
    ConstRawReader<ColorStorageType<ColorMode>, ColorMode::bits_per_pixel,
                   byte_order>
        read;
    return read(p, offset);
  }
};

}  // namespace internal

// The raster does not own its buffer. The representation of a raster it
// reasonably small (14-18 bytes, depending on the color mode), and can be
// passed by value.
template <typename PtrType, typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
class Raster : public Rasterizable {
 public:
  typedef RasterPixelStream<MemoryResource<PtrType>, ColorMode, pixel_order,
                            byte_order>
      StreamType;

  Raster(int16_t width, int16_t height, PtrType ptr,
         const ColorMode& color_mode = ColorMode())
      : Raster(Box(0, 0, width - 1, height - 1), ptr, std::move(color_mode)) {}

  Raster(Box extents, PtrType ptr, const ColorMode& color_mode = ColorMode())
      : extents_(std::move(extents)),
        ptr_(ptr),
        color_mode_(color_mode),
        width_(extents.width()) {}

  Box extents() const override { return extents_; }

  const ColorMode& color_mode() const { return color_mode_; }

  std::unique_ptr<StreamType> CreateRawStream() const {
    MemoryStream<PtrType> s(ptr_);
    return std::unique_ptr<StreamType>(new StreamType(s, color_mode_));
  }

  std::unique_ptr<PixelStream> CreateStream() const override {
    MemoryStream<PtrType> s(ptr_);
    return std::unique_ptr<PixelStream>(new StreamType(s, color_mode_));
  }

  std::unique_ptr<PixelStream> CreateStream(const Box& bounds) const override {
    MemoryStream<PtrType> s(ptr_);
    return SubRectangle(StreamType(s, color_mode_), extents(), bounds);
  }

  void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    internal::Reader<ColorMode, pixel_order, byte_order> read;
    while (count-- > 0) {
      *result++ = color_mode_.toArgbColor(read(
          ptr_, *x++ - extents_.xMin() + (*y++ - extents_.yMin()) * width_));
    }
  }

  TransparencyMode GetTransparencyMode() const override {
    return transparency();
  }

  TransparencyMode transparency() const { return color_mode_.transparency(); }

  Color get(int16_t x, int16_t y) const {
    internal::Reader<ColorMode, pixel_order, byte_order> read;
    return color_mode_.toArgbColor(
        read(ptr_, x - extents_.xMin() + (y - extents_.yMin()) * width_));
  }

 private:
  Box extents_;
  PtrType ptr_;
  ColorMode color_mode_;
  int16_t width_;
};

template <typename PtrType, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterArgb8888 =
    Raster<PtrType, Argb8888, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename PtrType, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterArgb6666 =
    Raster<PtrType, Argb6666, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename PtrType, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterArgb4444 =
    Raster<PtrType, Argb4444, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename PtrType, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterRgb565 =
    Raster<PtrType, Rgb565, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename PtrType, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using RasterRgb565WithTransparency =
    Raster<PtrType, Rgb565WithTransparency, COLOR_PIXEL_ORDER_MSB_FIRST,
           byte_order>;

template <typename PtrType>
using RasterGrayscale8 =
    Raster<PtrType, Grayscale8, COLOR_PIXEL_ORDER_MSB_FIRST,
           BYTE_ORDER_BIG_ENDIAN>;

template <typename PtrType>
using RasterAlpha8 =
    Raster<PtrType, Alpha8, COLOR_PIXEL_ORDER_MSB_FIRST, BYTE_ORDER_BIG_ENDIAN>;

template <typename PtrType,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using RasterGrayscale4 =
    Raster<PtrType, Grayscale4, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

template <typename PtrType,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using RasterAlpha4 =
    Raster<PtrType, Alpha4, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

template <typename PtrType,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using RasterMonochrome =
    Raster<PtrType, Monochrome, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

}  // namespace roo_display
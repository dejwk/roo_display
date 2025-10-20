#pragma once

#include "roo_display/color/color.h"
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/internal/byte_order.h"
#include "roo_display/internal/color_subpixel.h"
#include "roo_io/data/byte_order.h"
#include "roo_io/data/read.h"
#include "roo_io/memory/load.h"
#include "roo_io/memory/memory_iterable.h"

namespace roo_display {

template <typename Resource>
using StreamType = decltype(std::declval<Resource>().iterator());

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
  RasterPixelStream(StreamType<Resource> stream, const ColorMode& color_mode)
      : stream_(std::move(stream)),
        pixel_index_(ColorTraits<ColorMode>::pixels_per_byte),
        color_mode_(color_mode) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) {
      *buf++ = next();
    }
  }

  void Skip(uint32_t count) override { skip(count); }

  // Advances the iterator to the next pixel in the buffer.
  Color next() {
    if (pixel_index_ == ColorTraits<ColorMode>::pixels_per_byte) {
      pixel_index_ = 0;
      fetch();
    }
    return cache_[pixel_index_++];
  }

  void skip(uint32_t count) {
    uint32_t new_pixel_index = count + pixel_index_;
    if (new_pixel_index <= ColorTraits<ColorMode>::pixels_per_byte) {
      pixel_index_ = new_pixel_index;
      return;
    }
    uint32_t bytes_to_skip =
        new_pixel_index / ColorTraits<ColorMode>::pixels_per_byte - 1;
    if (bytes_to_skip > 0) {
      stream_.skip(bytes_to_skip);
    }
    pixel_index_ = new_pixel_index % ColorTraits<ColorMode>::pixels_per_byte;
    if (pixel_index_ == 0) {
      pixel_index_ = ColorTraits<ColorMode>::pixels_per_byte;
    } else {
      fetch();
    }
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
    return color_mode.toArgbColor(roo_io::ReadU8(in));
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 2> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    return color_mode.toArgbColor(roo_io::ReadU16<ByteStream, byte_order>(in));
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 3> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    return color_mode.toArgbColor(roo_io::ReadU24<ByteStream, byte_order>(in));
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 4> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    return color_mode.toArgbColor(roo_io::ReadU32<ByteStream, byte_order>(in));
  }
};

// Specialization for color multibyte-pixel color modes (e.g. RGB565).
template <typename Resource, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
class RasterPixelStream<Resource, ColorMode, pixel_order, byte_order, 1>
    : public PixelStream {
 public:
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
    stream_.skip(count * ColorMode::bits_per_pixel / 8);
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
  storage_type operator()(const roo::byte* p, uint32_t offset) const {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    const roo::byte* target = p + offset / pixels_per_byte;
    return subpixel.ReadSubPixelColor(*target, pixel_index);
  }
};

template <typename RawType, int bits_per_pixel, ByteOrder byte_order>
struct ConstRawReader {
  RawType operator()(const roo::byte* ptr, uint32_t offset) const {
    return roo_io::LoadInteger<byte_order, RawType>(ptr +
                                                    offset * sizeof(RawType));
  }
};

template <>
struct ConstRawReader<uint32_t, 24, roo_io::kBigEndian> {
  uint32_t operator()(const roo::byte* ptr, uint32_t offset) const {
    ptr += 3 * offset;
    return roo_io::LoadBeU24(ptr);
  }
};

template <>
struct ConstRawReader<uint32_t, 24, roo_io::kLittleEndian> {
  uint32_t operator()(const roo::byte* ptr, uint32_t offset) const {
    ptr += 3 * offset;
    return roo_io::LoadLeU24(ptr);
  }
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
struct Reader<ColorMode, pixel_order, byte_order, 1, storage_type> {
  storage_type operator()(const roo::byte* p, uint32_t offset) const {
    ConstRawReader<ColorStorageType<ColorMode>, ColorMode::bits_per_pixel,
                   byte_order>
        read;
    return read(p, offset);
  }
};

template <typename PtrTypeT>
struct PtrTypeResolver {
  using PtrType = PtrTypeT;
};

// To accommodate legacy generated raster definitions using uint8_t explicitly.
template <>
struct PtrTypeResolver<const uint8_t*> {
  using PtrType = const roo::byte*;
};

}  // namespace internal

// The raster does not own its buffer. The representation of a raster it
// reasonably small (22-26 bytes, depending on the color mode), and can be
// passed by value.
//
// Don't use explicitly; prefer either of DramRaster, ConstDramRaster, or
// ProgMemRaster.
template <typename PtrTypeT, typename ColorModeT,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
class Raster : public Rasterizable {
 public:
  using ColorMode = ColorModeT;
  using PtrType = typename internal::PtrTypeResolver<PtrTypeT>::PtrType;

  using StreamType =
      RasterPixelStream<roo_io::UnsafeGenericMemoryIterable<PtrType>, ColorMode,
                        pixel_order, byte_order>;

  using Reader = internal::Reader<ColorMode, pixel_order, byte_order>;

  Raster(int16_t width, int16_t height, PtrTypeT ptr,
         const ColorMode& color_mode = ColorMode())
      : Raster(Box(0, 0, width - 1, height - 1),
               Box(0, 0, width - 1, height - 1), ptr, std::move(color_mode)) {}

  Raster(int16_t width, int16_t height, Box extents, PtrTypeT ptr,
         const ColorMode& color_mode = ColorMode())
      : Raster(extents, Box(0, 0, width - 1, height - 1), ptr,
               std::move(color_mode)) {}

  Raster(Box extents, PtrTypeT ptr, const ColorMode& color_mode = ColorMode())
      : Raster(extents, extents, ptr, std::move(color_mode)) {}

  Raster(Box extents, Box anchor_extents, PtrTypeT ptr,
         const ColorMode& color_mode = ColorMode())
      : extents_(std::move(extents)),
        anchor_extents_(std::move(anchor_extents)),
        ptr_((PtrType)ptr),
        color_mode_(color_mode),
        width_(extents.width()) {}

  Box extents() const override { return extents_; }

  Box anchorExtents() const override { return anchor_extents_; }

  ColorMode& color_mode() { return color_mode_; }
  const ColorMode& color_mode() const { return color_mode_; }

  std::unique_ptr<StreamType> createRawStream() const {
    return std::unique_ptr<StreamType>(new StreamType(
        roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_), color_mode_));
  }

  std::unique_ptr<PixelStream> createStream() const override {
    return std::unique_ptr<PixelStream>(new StreamType(
        roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_), color_mode_));
  }

  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override {
    return SubRectangle(
        StreamType(roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_),
                   color_mode_),
        extents(), bounds);
  }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    Reader read;
    while (count-- > 0) {
      *result++ = color_mode_.toArgbColor(read(
          ptr_, *x++ - extents_.xMin() + (*y++ - extents_.yMin()) * width_));
    }
  }

  TransparencyMode getTransparencyMode() const override {
    return transparency();
  }

  TransparencyMode transparency() const { return color_mode_.transparency(); }

  Color get(int16_t x, int16_t y) const {
    Reader read;
    return color_mode_.toArgbColor(
        read(ptr_, x - extents_.xMin() + (y - extents_.yMin()) * width_));
  }

  const PtrType buffer() const { return ptr_; }

 private:
  void drawTo(const Surface& s) const override {
    Box bounds =
        Box::Intersect(s.clip_box().translate(-s.dx(), -s.dy()), extents_);
    if (bounds.empty()) return;
    if (extents_.width() == bounds.width() &&
        extents_.height() == bounds.height()) {
      StreamType stream(roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_),
                        color_mode_);
      internal::FillRectFromStream(s.out(), bounds.translate(s.dx(), s.dy()),
                                   &stream, s.bgcolor(), s.fill_mode(),
                                   s.blending_mode(), getTransparencyMode());
    } else {
      auto stream = internal::MakeSubRectangle(
          StreamType(roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_),
                     color_mode_),
          extents_, bounds);
      internal::FillRectFromStream(s.out(), bounds.translate(s.dx(), s.dy()),
                                   &stream, s.bgcolor(), s.fill_mode(),
                                   s.blending_mode(), getTransparencyMode());
    }
  }

  Box extents_;
  Box anchor_extents_;
  PtrType ptr_;
  ColorMode color_mode_;
  int16_t width_;
};

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
using DramRaster = Raster<roo::byte*, ColorMode, pixel_order, byte_order>;

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
using ConstDramRaster =
    Raster<const roo::byte*, ColorMode, pixel_order, byte_order>;

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
using ProgMemRaster =
    Raster<const roo::byte * PROGMEM, ColorMode, pixel_order, byte_order>;

template <typename ColorMode>
using DramRasterBE = Raster<roo::byte*, ColorMode,
                            COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kBigEndian>;

template <typename ColorMode>
using DramRasterLE = Raster<roo::byte*, ColorMode,
                            COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kLittleEndian>;

template <typename ColorMode>
using ConstDramRasterBE =
    Raster<const roo::byte*, ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST,
           roo_io::kBigEndian>;

template <typename ColorMode>
using ConstDramRasterLE =
    Raster<const roo::byte*, ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST,
           roo_io::kLittleEndian>;

template <typename ColorMode>
using ProgMemRasterBE = Raster<const roo::byte PROGMEM*, ColorMode,
                               COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kBigEndian>;

template <typename ColorMode>
using ProgMemRasterLE =
    Raster<const roo::byte PROGMEM*, ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST,
           roo_io::kLittleEndian>;

}  // namespace roo_display
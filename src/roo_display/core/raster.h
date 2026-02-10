#pragma once

#include "roo_display/color/color.h"
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/hal/progmem.h"
#include "roo_display/internal/byte_order.h"
#include "roo_display/internal/color_format.h"
#include "roo_display/internal/color_io.h"
#include "roo_io/data/byte_order.h"
#include "roo_io/data/read.h"
#include "roo_io/memory/load.h"
#include "roo_io/memory/memory_iterable.h"

namespace roo_display {

/// Stream type produced by a resource iterable.
template <typename Resource>
using StreamType = decltype(std::declval<Resource>().iterator());

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order,
          int bytes_per_pixel = ColorTraits<ColorMode>::bytes_per_pixel>
struct ColorReader;

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 1> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    roo::byte buf[1];
    in.read(buf, 1);
    return ColorIo<ColorMode, byte_order>().load(buf, color_mode);
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 2> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    roo::byte buf[2];
    in.read(buf, 2);
    return ColorIo<ColorMode, byte_order>().load(buf, color_mode);
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 3> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    roo::byte buf[3];
    in.read(buf, 3);
    return ColorIo<ColorMode, byte_order>().load(buf, color_mode);
  }
};

template <typename ByteStream, typename ColorMode, ColorPixelOrder pixel_order,
          ByteOrder byte_order>
struct ColorReader<ByteStream, ColorMode, pixel_order, byte_order, 4> {
  Color operator()(ByteStream& in, const ColorMode& color_mode) const {
    roo::byte buf[4];
    in.read(buf, 4);
    return ColorIo<ColorMode, byte_order>().load(buf, color_mode);
  }
};

/// Pixel stream that reads from a raw byte resource.
///
/// Default implementation for color modes with multiple pixels per byte.
/// The resource is interpreted as a non-compressed stream of consecutive
/// pixels. Line/column semantics are handled by the caller.
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
    SubByteColorIo<ColorMode, pixel_order> io;
    io.loadBulk(color_mode_, stream_.read(), cache_);
  }

  StreamType<Resource> stream_;
  uint8_t pixel_index_;
  const ColorMode& color_mode_;
  Color cache_[ColorTraits<ColorMode>::pixels_per_byte];
};

/// Specialization for color modes with at least one byte per pixel.
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
  Color operator()(const roo::byte* p, uint32_t offset,
                   const ColorMode& color_mode) const {
    SubByteColorIo<ColorMode, pixel_order> io;
    int pixel_index = offset % pixels_per_byte;
    const roo::byte* target = p + offset / pixels_per_byte;
    return color_mode.toArgbColor(io.loadRaw(*target, pixel_index));
  }
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
struct Reader<ColorMode, pixel_order, byte_order, 1, storage_type> {
  Color operator()(const roo::byte* p, uint32_t offset,
                   const ColorMode& color_mode) const {
    const roo::byte* src = p + offset * ColorTraits<ColorMode>::bytes_per_pixel;
    return ColorIo<ColorMode, byte_order>().load(src, color_mode);
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

/// Non-owning raster view over a pixel buffer.
///
/// The raster representation is small and can be passed by value. Prefer using
/// `DramRaster`, `ConstDramRaster`, or `ProgMemRaster` aliases instead of this
/// template directly.
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

  /// Construct a raster for a width/height buffer.
  Raster(int16_t width, int16_t height, PtrTypeT ptr,
         const ColorMode& color_mode = ColorMode())
      : Raster(Box(0, 0, width - 1, height - 1),
               Box(0, 0, width - 1, height - 1), ptr, std::move(color_mode)) {}

  /// Construct a raster with custom extents and buffer dimensions.
  Raster(int16_t width, int16_t height, Box extents, PtrTypeT ptr,
         const ColorMode& color_mode = ColorMode())
      : Raster(extents, Box(0, 0, width - 1, height - 1), ptr,
               std::move(color_mode)) {}

  /// Construct a raster with extents and a buffer pointer.
  Raster(Box extents, PtrTypeT ptr, const ColorMode& color_mode = ColorMode())
      : Raster(extents, extents, ptr, std::move(color_mode)) {}

  /// Construct a raster with extents and anchor extents.
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
      *result++ =
          read(ptr_, *x++ - extents_.xMin() + (*y++ - extents_.yMin()) * width_,
               color_mode_);
    }
  }

  TransparencyMode getTransparencyMode() const override {
    return transparency();
  }

  TransparencyMode transparency() const { return color_mode_.transparency(); }

  Color get(int16_t x, int16_t y) const {
    Reader read;
    return read(ptr_, x - extents_.xMin() + (y - extents_.yMin()) * width_,
                color_mode_);
  }

  const PtrType buffer() const { return ptr_; }

 private:
  void drawTo(const Surface& s) const override {
    Box bounds =
        Box::Intersect(s.clip_box().translate(-s.dx(), -s.dy()), extents_);
    if (bounds.empty()) return;
    TransparencyMode source_transparency = getTransparencyMode();
    bool source_opaque = (source_transparency == TRANSPARENCY_NONE);
    bool replace_ok = source_opaque || (s.fill_mode() == FILL_MODE_RECTANGLE &&
                                        s.bgcolor().a() == 0);
    if (replace_ok) {
      const DisplayOutput::ColorFormat& format = s.out().getColorFormat();
      // Fast format check: only proceed when format, pixel, and byte order
      // match.
      bool format_match =
          format.mode() == internal::ColorFormatTraits<ColorMode>::mode &&
          format.pixel_order() == pixel_order &&
          format.byte_order() == byte_order;
      if (format_match) {
        BlendingMode mode = s.blending_mode();
        // Porter-Duff folding: source-over is source when the source is opaque.
        bool blend_is_source = mode == BLENDING_MODE_SOURCE ||
                               ((mode == BLENDING_MODE_SOURCE_OVER ||
                                 mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) &&
                                source_opaque);
        // When both source and destination are opaque, SOURCE_IN/SOURCE_ATOP
        // reduce to a straight source copy as well.
        if (!blend_is_source && source_opaque &&
            format.transparency() == TRANSPARENCY_NONE &&
            (mode == BLENDING_MODE_SOURCE_IN ||
             mode == BLENDING_MODE_SOURCE_ATOP)) {
          blend_is_source = true;
        }
        if (blend_is_source) {
          int16_t src_x0 = bounds.xMin() - extents_.xMin();
          int16_t src_y0 = bounds.yMin() - extents_.yMin();
          int16_t src_x1 = bounds.xMax() - extents_.xMin();
          int16_t src_y1 = bounds.yMax() - extents_.yMin();
          int16_t dst_x0 = bounds.xMin() + s.dx();
          int16_t dst_y0 = bounds.yMin() + s.dy();
          size_t row_width_bytes;
          if constexpr (ColorTraits<ColorMode>::pixels_per_byte == 1) {
            row_width_bytes = width_ * ColorTraits<ColorMode>::bytes_per_pixel;
          } else {
            row_width_bytes =
                (width_ + ColorTraits<ColorMode>::pixels_per_byte - 1) /
                ColorTraits<ColorMode>::pixels_per_byte;
          }
          s.out().drawDirectRect(ptr_, row_width_bytes, src_x0, src_y0, src_x1,
                                 src_y1, dst_x0, dst_y0);
          return;
        }
      }
    }
    if (extents_.width() == bounds.width() &&
        extents_.height() == bounds.height()) {
      StreamType stream(roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_),
                        color_mode_);
      internal::FillRectFromStream(s.out(), bounds.translate(s.dx(), s.dy()),
                                   &stream, s.bgcolor(), s.fill_mode(),
                                   s.blending_mode(), source_transparency);
    } else {
      auto stream = internal::MakeSubRectangle(
          StreamType(roo_io::UnsafeGenericMemoryIterator<PtrType>(ptr_),
                     color_mode_),
          extents_, bounds);
      internal::FillRectFromStream(s.out(), bounds.translate(s.dx(), s.dy()),
                                   &stream, s.bgcolor(), s.fill_mode(),
                                   s.blending_mode(), source_transparency);
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
class DramRaster
    : public Raster<roo::byte*, ColorMode, pixel_order, byte_order> {
 public:
  using Raster<roo::byte*, ColorMode, pixel_order, byte_order>::Raster;

  DramRaster(int16_t width, int16_t height, uint8_t* ptr,
             const ColorMode& color_mode = ColorMode())
      : Raster<roo::byte*, ColorMode, pixel_order, byte_order>(
            width, height, (roo::byte*)ptr, color_mode) {}

  DramRaster(int16_t width, int16_t height, Box extents, uint8_t* ptr,
             const ColorMode& color_mode = ColorMode())
      : Raster<roo::byte*, ColorMode, pixel_order, byte_order>(
            width, height, std::move(extents), (roo::byte*)ptr, color_mode) {}

  DramRaster(Box extents, uint8_t* ptr,
             const ColorMode& color_mode = ColorMode())
      : Raster<roo::byte*, ColorMode, pixel_order, byte_order>(
            std::move(extents), extents, (roo::byte*)ptr, color_mode) {}

  DramRaster(Box extents, Box anchor_extents, uint8_t* ptr,
             const ColorMode& color_mode = ColorMode())
      : Raster<roo::byte*, ColorMode, pixel_order, byte_order>(
            std::move(extents), std::move(anchor_extents), (roo::byte*)ptr,
            color_mode) {}
};

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
class ConstDramRaster
    : public Raster<const roo::byte*, ColorMode, pixel_order, byte_order> {
 public:
  using Raster<const roo::byte*, ColorMode, pixel_order, byte_order>::Raster;

  ConstDramRaster(int16_t width, int16_t height, const uint8_t* ptr,
                  const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte*, ColorMode, pixel_order, byte_order>(
            width, height, (const roo::byte*)ptr, color_mode) {}

  ConstDramRaster(int16_t width, int16_t height, Box extents,
                  const uint8_t* ptr, const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte*, ColorMode, pixel_order, byte_order>(
            width, height, std::move(extents), (const roo::byte*)ptr,
            color_mode) {}

  ConstDramRaster(Box extents, const uint8_t* ptr,
                  const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte*, ColorMode, pixel_order, byte_order>(
            std::move(extents), extents, (const roo::byte*)ptr, color_mode) {}

  ConstDramRaster(Box extents, Box anchor_extents, const uint8_t* ptr,
                  const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte*, ColorMode, pixel_order, byte_order>(
            std::move(extents), std::move(anchor_extents),
            (const roo::byte*)ptr, color_mode) {}
};

template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
class ProgMemRaster : public Raster<const roo::byte * PROGMEM, ColorMode,
                                    pixel_order, byte_order> {
 public:
  using Raster<const roo::byte * PROGMEM, ColorMode, pixel_order,
               byte_order>::Raster;

  ProgMemRaster(int16_t width, int16_t height, const uint8_t* PROGMEM ptr,
                const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte * PROGMEM, ColorMode, pixel_order, byte_order>(
            width, height, (const roo::byte*)ptr, color_mode) {}

  ProgMemRaster(int16_t width, int16_t height, Box extents,
                const uint8_t* PROGMEM ptr,
                const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte * PROGMEM, ColorMode, pixel_order, byte_order>(
            width, height, std::move(extents), (const roo::byte*)ptr,
            color_mode) {}

  ProgMemRaster(Box extents, const uint8_t* PROGMEM ptr,
                const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte * PROGMEM, ColorMode, pixel_order, byte_order>(
            std::move(extents), extents, (const roo::byte*)ptr, color_mode) {}

  ProgMemRaster(Box extents, Box anchor_extents, const uint8_t* PROGMEM ptr,
                const ColorMode& color_mode = ColorMode())
      : Raster<const roo::byte * PROGMEM, ColorMode, pixel_order, byte_order>(
            std::move(extents), std::move(anchor_extents),
            (const roo::byte*)ptr, color_mode) {}
};

template <typename ColorMode>
using DramRasterBE =
    DramRaster<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kBigEndian>;

template <typename ColorMode>
using DramRasterLE =
    DramRaster<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kLittleEndian>;

template <typename ColorMode>
using ConstDramRasterBE =
    ConstDramRaster<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kBigEndian>;

template <typename ColorMode>
using ConstDramRasterLE =
    ConstDramRaster<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST,
                    roo_io::kLittleEndian>;

template <typename ColorMode>
using ProgMemRasterBE =
    ProgMemRaster<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST, roo_io::kBigEndian>;

template <typename ColorMode>
using ProgMemRasterLE = ProgMemRaster<ColorMode, COLOR_PIXEL_ORDER_MSB_FIRST,
                                      roo_io::kLittleEndian>;

}  // namespace roo_display
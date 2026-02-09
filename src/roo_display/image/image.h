#pragma once

#include "roo_display/color/color.h"
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/named.h"
#include "roo_display/color/pixel_order.h"
#include "roo_display/core/streamable.h"
#include "roo_display/image/image_stream.h"
#include "roo_display/internal/raw_streamable.h"
#include "roo_display/io/memory.h"

/// Image helpers and type aliases.
///
/// Images are drawables that render a rectangular area based on data from an
/// immutable data source. Image classes are templated on `Resource`, which
/// specifies the input source (e.g., DRAM, flash, SD card). Resource helpers
/// live in the io/ directory, but you can define your own.
///
/// Each image alias recognizes a specific data format (e.g., raster bitmap or
/// RLE-encoded data). Every image is a `Drawable`; many are `Streamable`, and
/// some may be `Rasterizable`.

/// Streamable and Rasterizable images can be alpha-blended without full
/// buffering. Common use: semi-transparent images over solid backgrounds.
//
/// Note: for uncompressed images in native color modes, use `Raster`.
//
/// How to implement a new Streamable image class:
/// 1) Implement a stream type with `Color next()` and `skip(int)`.
/// 2) For full-stream images backed by memory, typedef
///    `SimpleStreamable<YourStreamType>`.
/// 3) For clipped sub-rectangles, use `Clipping<SimpleStreamable<...>>`.
/// 4) For custom constructors, inherit and delegate (see `XBitmap`).

namespace roo_display {

/// Run-length-encoded image for color modes with >= 8 bits per pixel.
template <typename ColorMode, typename Resource = ProgMemPtr>
using RleImage =
    SimpleStreamable<Resource, ColorMode,
                     internal::RleStreamUniform<Resource, ColorMode>>;

/// RLE 4-bit image with bias for extreme values (0x0/0xF).
template <typename ColorMode, typename Resource = ProgMemPtr>
using RleImage4bppxBiased =
    SimpleStreamable<Resource, ColorMode,
                     internal::RleStream4bppxBiased<Resource, ColorMode>>;

/// Convenience alias for small anti-aliased monochrome artwork.
using Pictogram = RleImage4bppxBiased<Alpha4>;

/// Uncompressed image.
template <typename Resource, typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = roo_io::kBigEndian>
using SimpleImage = SimpleStreamable<
    Resource, ColorMode,
    RasterPixelStream<Resource, ColorMode, pixel_order, byte_order>>;

/// Uncompressed ARGB8888 image.
template <typename Resource, ByteOrder byte_order = roo_io::kBigEndian>
using SimpleImageArgb8888 =
    SimpleImage<Resource, Argb8888, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

/// Uncompressed ARGB6666 image.
template <typename Resource, ByteOrder byte_order = roo_io::kBigEndian>
using SimpleImageArgb6666 =
    SimpleImage<Resource, Argb6666, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

/// Uncompressed ARGB4444 image.
template <typename Resource, ByteOrder byte_order = roo_io::kBigEndian>
using SimpleImageArgb4444 =
    SimpleImage<Resource, Argb4444, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

/// Uncompressed RGB565 image.
template <typename Resource, ByteOrder byte_order = roo_io::kBigEndian>
using SimpleImageRgb565 =
    SimpleImage<Resource, Rgb565, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

/// Uncompressed RGB565 image with reserved transparency.
template <typename Resource, ByteOrder byte_order = roo_io::kBigEndian>
using SimpleImageRgb565WithTransparency =
    SimpleImage<Resource, Rgb565WithTransparency, COLOR_PIXEL_ORDER_MSB_FIRST,
                byte_order>;

/// Uncompressed Grayscale8 image.
template <typename Resource>
using SimpleImageGrayscale8 =
    SimpleImage<Resource, Grayscale8, COLOR_PIXEL_ORDER_MSB_FIRST,
                roo_io::kBigEndian>;

/// Uncompressed Alpha8 image.
template <typename Resource>
using SimpleImageAlpha8 =
    SimpleImage<Resource, Alpha8, COLOR_PIXEL_ORDER_MSB_FIRST,
                roo_io::kBigEndian>;

/// Uncompressed Grayscale4 image.
template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using SimpleImageGrayscale4 =
    SimpleImage<Resource, Grayscale4, pixel_order, roo_io::kBigEndian>;

/// Uncompressed Alpha4 image.
template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using SimpleImageAlpha4 =
    SimpleImage<Resource, Alpha4, pixel_order, roo_io::kBigEndian>;

/// Uncompressed monochrome image.
template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using SimpleImageMonochrome =
    SimpleImage<Resource, Monochrome, pixel_order, roo_io::kBigEndian>;

/// Monochrome XBitmap image (rows rounded to 8 pixels).
template <typename Resource = ProgMemPtr>
class XBitmap
    : public Clipping<
          SimpleImage<Resource, Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST,
                      roo_io::kBigEndian>> {
 public:
  typedef Clipping<SimpleImage<Resource, Monochrome,
                               COLOR_PIXEL_ORDER_LSB_FIRST, roo_io::kBigEndian>>
      Base;

  XBitmap(int16_t width, int16_t height, const Resource& input, Color fg,
          Color bg = color::Transparent)
      : Base(Box(0, 0, width - 1, height - 1), ((width + 7) / 8) * 8, height,
             std::move(input), Monochrome(fg, bg)) {}
};

}  // namespace roo_display
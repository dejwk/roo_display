#pragma once

#include "roo_display/core/streamable.h"
#include "roo_display/image/image_stream.h"
#include "roo_display/internal/raw_streamable.h"

// Images are drawables that render a rectangular area based on data from some
// underlying immutable data source. Image classes are templated on Resource,
// which specifies the source of input data, such as DRAM memory, flash memory,
// or SD card. Specific Resource classes are included in the io/ subdictory; you
// could also roll your own based on those.
//
// Each image class recognizes a particular image data format, such as
// raster bitmap, or RLE-encoded image data.
//
// Images do not have a common base class. Every image can be used as a
// Drawable; many are Streamable (see core/streamable.h), and some might be
// Rasterizable (see core/rasterizable.h).

// Streamable and Rasterizable images can be super-imposed (and alpha-blended)
// over each other, and over other Streamables, without needing to store the
// entire content in DRAM. A common use case is to super-impose semi-transparent
// images over solid backgrounds. For example, you can define a button with a
// single image label, and varying background color depending on button state.
//
// Note: for simple raster (uncompressed) images, stored in native ColorMode
// formats, use Raster from core/raster.h.
//
// How to implement a new Streamable image class:
// 1. Implement a new 'stream' type, with methods Color next() and skip(int),
//    that will return pixels of your image from the top-left corner
// 2. In a typical case when your image is rendering the entire stream
//    and is backed by a memory pointer, it generally suffices to typedef
//    it as SimpleStreamable<YourStreamType>
// 3. If the image represents a clipped sub-rectangle of the stream, use
//    ClippedStreamable instead of SimpleStreamable.
// 4. If you need a more customized constructor, inherit rather than
//    typedef, and delegate your constructor. See XBitmap, below.

namespace roo_display {

// Run-length-encoded image, for color modes with >= 8 bits_per_pixel.
template <typename ColorMode, typename Resource = ProgMemPtr>
using RleImage =
    SimpleStreamable<Resource, ColorMode,
                     internal::RleStreamUniform<Resource, ColorMode>>;

// Run-length-encoded 4-bit image, for color modes with 4 bits_per_pixel, with
// preferrential RLE encoding for extreme values (0x0 and 0xF). Particularly
// useful with Alpha4, e.g. for font glyphs.
template <typename ColorMode, typename Resource = ProgMemPtr>
using RleImage4bppxBiased =
    SimpleStreamable<Resource, ColorMode,
                     internal::RleStream4bppxBiased<Resource, ColorMode>>;

// Uncompressed image.
template <typename Resource, typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using SimpleImage = SimpleStreamable<
    Resource, ColorMode,
    RasterPixelStream<Resource, ColorMode, pixel_order, byte_order>>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using SimpleImageArgb8888 =
    SimpleImage<Resource, Argb8888, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using SimpleImageArgb6666 =
    SimpleImage<Resource, Argb6666, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using SimpleImageArgb4444 =
    SimpleImage<Resource, Argb4444, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using SimpleImageRgb565 =
    SimpleImage<Resource, Rgb565, COLOR_PIXEL_ORDER_MSB_FIRST, byte_order>;

template <typename Resource, ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN>
using SimpleImageRgb565WithTransparency =
    SimpleImage<Resource, Rgb565WithTransparency, COLOR_PIXEL_ORDER_MSB_FIRST,
                byte_order>;

template <typename Resource>
using SimpleImageGrayscale8 =
    SimpleImage<Resource, Grayscale8, COLOR_PIXEL_ORDER_MSB_FIRST,
                BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource>
using SimpleImageAlpha8 =
    SimpleImage<Resource, Alpha8, COLOR_PIXEL_ORDER_MSB_FIRST,
                BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using SimpleImageGrayscale4 =
    SimpleImage<Resource, Grayscale4, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using SimpleImageAlpha4 =
    SimpleImage<Resource, Alpha4, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

template <typename Resource,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST>
using SimpleImageMonochrome =
    SimpleImage<Resource, Monochrome, pixel_order, BYTE_ORDER_BIG_ENDIAN>;

// A simple monochrome bit raster. Lines rounded to 8 pixel width.
template <typename Resource = ProgMemPtr>
class XBitmap
    : public Clipping<
          SimpleImage<Resource, Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST,
                      BYTE_ORDER_NATIVE>> {
 public:
  typedef Clipping<SimpleImage<Resource, Monochrome,
                               COLOR_PIXEL_ORDER_LSB_FIRST, BYTE_ORDER_NATIVE>>
      Base;

  XBitmap(int16_t width, int16_t height, const Resource& input, Color fg,
          Color bg = color::Transparent)
      : Base(Box(0, 0, width - 1, height - 1), ((width + 7) / 8) * 8, height,
             std::move(input), Monochrome(fg, bg)) {}
};

}  // namespace roo_display
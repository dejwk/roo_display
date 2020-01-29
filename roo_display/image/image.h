#pragma once

#include "roo_display/image/image_stream.h"

// Image is a Streamable that renders a rectangular area based on data from some
// underlying data source. Image classes are templated on Resource, which
// specifies the source of input data, such as DRAM memory, flash memory, or SD
// card. Specific Resource classes are included in the io/ subdictory; you could
// also roll your own based on those.
//
// Each image class recognizes a particular image data format, such as
// raster bitmap, or RLE-encoded image data.
//
// Because Images are Streamables (see core/streamable.h), you can
// super-impose (and alpha-blend) images over each other, and over other
// Streamables, without needing to store the entire content in DRAM. A common
// use case is to super-impose semi-transparent images over solid backgrounds.
// For example, you can define a button with a single image label, and varying
// background color depending on button state.
//
// Note: for simple raster (uncompressed) images, stored in native ColorMode
// formats, use Raster from core/raster.h.
//
// How to implement a new image class:
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

// A simple monochrome bit raster. Lines rounded to 8 pixel width.
template <typename Resource = PrgMemResource>
class XBitmap
    : public Clipping<Raster<Resource, Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST,
                             BYTE_ORDER_NATIVE>> {
 public:
  typedef Clipping<Raster<Resource, Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST,
                          BYTE_ORDER_NATIVE>>
      Base;

  XBitmap(int16_t width, int16_t height, const Resource& input, Color fg,
          Color bg = color::Transparent)
      : Base(Box(0, 0, width - 1, height - 1), ((width + 7) / 8) * 8, height,
             std::move(input), Monochrome(fg, bg)) {}
};

// Run-length-encoded image, for color modes with >= 8 bits_per_pixel.
template <typename ColorMode, typename Resource = PrgMemResource>
using RleImage =
    SimpleStreamable<Resource, ColorMode,
                     internal::RleStreamUniform<Resource, ColorMode>>;

// Run-length-encoded 4-bit image, for color modes with 4 bits_per_pixel, with
// preferrential RLE encoding for extreme values (0x0 and 0xF). Particularly
// useful with Alpha4, e.g. for font glyphs.
template <typename ColorMode, typename Resource = PrgMemResource>
using RleImage4bppxPolarized =
    SimpleStreamable<Resource, ColorMode,
                     internal::RleStream4bppxPolarized<Resource, ColorMode>>;

// // Run-length-encoded image in RGB565 and an addiional 4-bit alpha channel.
// template <typename Resource = PrgMemResource>
// using Rgb565Alpha4RleImage =
//     SimpleStreamable<Resource, Argb8888,
//                      internal::Rgb565Alpha4RleStream<Resource>>;

}  // namespace roo_display
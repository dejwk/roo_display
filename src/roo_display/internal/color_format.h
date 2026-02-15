#pragma once

#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/pixel_order.h"
#include "roo_display/core/device.h"
#include "roo_display/internal/color_io.h"
#include "roo_io/data/byte_order.h"

namespace roo_display {
namespace internal {

template <typename ColorMode>
struct ColorFormatTraits;

template <>
struct ColorFormatTraits<Rgb565> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeRgb565;
};

template <>
struct ColorFormatTraits<Rgb565WithTransparency> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeRgb565;
};

template <>
struct ColorFormatTraits<Rgba8888> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeRgba8888;
};

template <>
struct ColorFormatTraits<Rgb888> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeRgb888;
};

template <>
struct ColorFormatTraits<Argb6666> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeArgb6666;
};

template <>
struct ColorFormatTraits<Argb4444> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeArgb4444;
};

template <>
struct ColorFormatTraits<Argb8888> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeArgb8888;
};

template <>
struct ColorFormatTraits<Grayscale8> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeGrayscale8;
};

template <>
struct ColorFormatTraits<GrayAlpha8> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeGrayAlpha8;
};

template <>
struct ColorFormatTraits<Grayscale4> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeGrayscale4;
};

template <>
struct ColorFormatTraits<Alpha8> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeAlpha8;
};

template <>
struct ColorFormatTraits<Alpha4> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeAlpha4;
};

template <>
struct ColorFormatTraits<Monochrome> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeMonochrome;
};

template <>
struct ColorFormatTraits<Indexed<1>> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeIndexed1;
};

template <>
struct ColorFormatTraits<Indexed<2>> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeIndexed2;
};

template <>
struct ColorFormatTraits<Indexed<4>> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeIndexed4;
};

template <>
struct ColorFormatTraits<Indexed<8>> {
  static constexpr const DisplayOutput::ColorFormat::Mode mode =
      DisplayOutput::ColorFormat::kModeIndexed8;
};

template <typename ColorMode, roo_io::ByteOrder kByteOrder,
          ColorPixelOrder kPixelOrder = COLOR_PIXEL_ORDER_MSB_FIRST>
class ColorFormatImpl : public DisplayOutput::ColorFormat {
 public:
  ColorFormatImpl(const ColorMode& mode)
      : DisplayOutput::ColorFormat(ColorFormatTraits<ColorMode>::mode,
                                   kByteOrder, kPixelOrder),
        mode_(mode) {}

  void decode(const roo::byte* data, size_t row_width_bytes, int16_t x0,
              int16_t y0, int16_t x1, int16_t y1,
              Color* output) const override {
    ColorRectIo<ColorMode, kByteOrder, kPixelOrder> io;
    io.decode(data, row_width_bytes, x0, y0, x1, y1, output, mode_);
  }

  bool decodeIfUniform(const roo::byte* data, size_t row_width_bytes,
                       int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       Color* output) const override {
    ColorRectIo<ColorMode, kByteOrder, kPixelOrder> io;
    return io.decodeIfUniform(data, row_width_bytes, x0, y0, x1, y1, output,
                              mode_);
  }

 private:
  const ColorMode& mode_;
};

}  // namespace internal
}  // namespace roo_display
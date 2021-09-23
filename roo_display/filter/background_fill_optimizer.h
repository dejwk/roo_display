#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/internal/memfill.h"

namespace roo_display {

static const int kBgFillOptimizerWindowSize = 4;

class NibbleRect;

// Display device filter which reduces the amount of redundant background
// re-drawing, for a collection of up to 15 designated background colors.
//
// A natural, most efficient way to avoid needless redrawing of identically
// colored pixel content is to use a frame buffer that 'remembers' the last
// color written for each of the device pixel. With such framebuffer, redundant
// redraws are completely eliminated - an attempt to write a pixel with the same
// color as previously, are silently discarded. Unfortunately, such a frame
// buffer requires significant RAM, which microcontrollers will usually not have
// enough to spare. For example, to buffer the contents of an ILI9486 display,
// we need 480 x 320 x 2 (width x height x bytes per pixel) = 300 KB of RAM.
//
// This device filter uses a compromise, capable of achieving a very significant
// reduction in redundant redraws, at a fraction of the memory footprint.
// Specifically, it focuses on use cases in which large areas are getting
// rect-filled using colors from a small palette. This is common in GUIs, that
// use solid backgrounds and some amount of solid area fills using a predefined
// set of colors. The device maintains a reduced-resolution framebuffer, whose
// each pixel corresponds to a rectangular area of the underlying device. Each
// pixel of the framebuffer can take one of 16 values. Values 1-15 mean that
// the corresponding rectangle in the underlying device is known to be entirely
// filled with the specific color from a 15-color palette. Value 0 means that
// this is not the case; i.e. that the corresponding rectangle either isn't
// known to be mono-color, or that it contains pixels with colors outside of
// that small palette. When 'Fill-rect' is called on this filter, using a color
// from the pre-specified 15-color palette, the implementation omits writes
// to the areas that are already known to be filled with the given color.
//
// With 4x resolution along both X and Y axes, i.e. when each pixel of the
// framebuffer corresponds to a 4x4 rectangle, we achieve 64-fold reduction in
// the RAM footprint. For example, the frame buffer for the aforementioned
// ILI9486 display now only uses 4800 bytes.
//
// This filter is meant to be used directly on top of a physical device. In
// particular, it expects the underlying display output to have zero offset.
// That is, the (0, 0) nibble of the background mask corresponds to the output
// rectangle (0, 0, kBgFillOptimizerWindowSize - 1, kBgFillOptimizerWindowSize -
// 1).
class BackgroundFillOptimizer : public DisplayOutput {
 public:
  // Creates the instance of a background fill optimizer filter, delegating to
  // the specified display output (which usually should be a physical device),
  // using the specified background mask, and a specified 15-color palette.
  // The color palette should not be modified after this filter is created.
  // The background_mask is expected to have sufficient size to cover the entire
  // area of the underlying display output. For example, For a display output
  // sized 480x320, and assuming the default kBkFillOptimizerWindowSize == 4,
  // the background mask should have the buffer of 4800 bytes, with width_bytes
  // = 480 / 4 / 2 (nibbles per byte) = 60, and height = 320 / 4 = 80.
  BackgroundFillOptimizer(DisplayOutput* output, NibbleRect* background_mask,
                          const Color* bg_palette);

  virtual ~BackgroundFillOptimizer() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override;

  void write(Color* color, uint32_t pixel_count) override;

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override;

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override;

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override;

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override;

 private:
  // Returns an int from 1 to 15 (inclusive) if the specified color is found in
  // the background palette, and 0 otherwise.
  inline uint8_t getIdxInPalette(Color color);

  void writePixel(int16_t x, int16_t y, Color c, BufferedPixelWriter* writer);

  template <typename Filler>
  void fillRectBg(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  Filler* filler, uint8_t palette_idx);

  DisplayOutput* output_;
  NibbleRect* background_mask_;
  const Color* palette_;

  Box address_window_;
  PaintMode paint_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

// Rectangular nibble (half-byte) area.
class NibbleRect {
 public:
  NibbleRect(uint8_t* buffer, int16_t width_bytes, int16_t height)
      : buffer_(buffer), width_bytes_(width_bytes), height_(height) {}

  uint8_t* buffer() { return buffer_; }
  const uint8_t* buffer() const { return buffer_; }
  int16_t width_bytes() const { return width_bytes_; }
  int16_t width() const { return width_bytes_ * 2; }
  int16_t height() const { return height_; }

  uint8_t get(int16_t x, int16_t y) const {
    return (buffer_[y * width_bytes_ + x / 2] >> ((1 - (x % 2)) * 4)) & 0xF;
  }

  void set(int16_t x, int16_t y, uint8_t value) {
    uint8_t& byte = buffer_[x / 2 + y * width_bytes()];
    if (x % 2 == 0) {
      byte &= 0x0F;
      byte |= (value << 4);
    } else {
      byte &= 0xF0;
      byte |= value;
    }
  }

  // Fills the specified rectangle of the mask with the specified bit value.
  void fillRect(const Box& rect, uint8_t value) {
    if (rect.xMin() == 0 && rect.xMax() == width() - 1) {
      nibble_fill(buffer_, rect.yMin() * width(), rect.height() * width(),
                  value);
    } else {
      int16_t w = rect.width();
      uint32_t offset = rect.xMin() + rect.yMin() * width();
      for (int16_t i = rect.height(); i > 0; --i) {
        nibble_fill(buffer_, offset, w, value);
        offset += width();
      }
    }
  }

 private:
  uint8_t* buffer_;
  int16_t width_bytes_;
  int16_t height_;
};

}  // namespace roo_display

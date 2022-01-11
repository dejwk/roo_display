#pragma once

#include "roo_display/core/color.h"
#include "roo_display/core/device.h"

namespace roo_display {

template <typename Filter>
class ColorFilter : public DisplayOutput {
 public:
  ColorFilter(DisplayOutput& output, Filter filter,
              Color bgcolor = color::Transparent)
      : output_(output), filter_(filter), bgcolor_(bgcolor) {}

  ColorFilter(DisplayOutput& output, Color bgcolor = color::Transparent)
      : ColorFilter(output, Filter(), bgcolor) {}

  virtual ~ColorFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    output_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    transform(color, pixel_count);
    output_.write(color, pixel_count);
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    transform(color, count);
    output_.writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    output_.fillRects(mode, transform(color), x0, y0, x1, y1, count);
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    transform(color, pixel_count);
    output_.writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    output_.fillPixels(mode, transform(color), x, y, pixel_count);
  }

 private:
  void transform(Color* colors, uint16_t count) const {
    for (uint16_t i = 0; i < count; i++) {
      colors[i] = transform(colors[i]);
    }
  }

  Color transform(Color c) const { return filter_(c, bgcolor_); }

  DisplayOutput& output_;
  Filter filter_;
  Color bgcolor_;
};

class Opaqueness {
 public:
  // Sets opaqueness of the input color. 127 = opaque (no effect). 0 = fully
  // transparent (equivalent to erasure).
  Opaqueness(uint8_t opaqueness) : opaqueness_(opaqueness) {}

  Color operator()(Color c, Color bg) const {
    c.set_a((c.a() * opaqueness_) >> 7);
    return alphaBlend(bg, c);
  }

 private:
  uint8_t opaqueness_;
};

class Erasure {
 public:
  // Always writes the bgcolor.
  Erasure() {}

  Color operator()(Color c, Color bg) const { return bg; }
};

// Specialization of ColorFilter for Erasure is able to avoid some memory
// copying and use more performant device operations.
template <>
class ColorFilter<Erasure> : public DisplayOutput {
 public:
  ColorFilter(DisplayOutput& output, Erasure filter,
              Color bgcolor = color::Transparent)
      : output_(output), bgcolor_(bgcolor) {}

  ColorFilter(DisplayOutput& output, Color bgcolor = color::Transparent)
      : ColorFilter(output, Erasure(), bgcolor) {}

  virtual ~ColorFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    output_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    for (uint16_t i = 0; i < pixel_count; i++) {
      color[i] = bgcolor_;
    }
    output_.write(color, pixel_count);
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    output_.fillRects(mode, bgcolor_, x0, y0, x1, y1, count);
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    output_.fillRects(mode, bgcolor_, x0, y0, x1, y1, count);
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    output_.fillPixels(mode, bgcolor_, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    output_.fillPixels(mode, bgcolor_, x, y, pixel_count);
  }

 private:
  DisplayOutput& output_;
  Color bgcolor_;
};

class Overlay {
 public:
  Overlay(Color color) : color_(color) {}

  Color operator()(Color c, Color bg) const {
    return alphaBlend(alphaBlend(bg, c), color_);
  }

 private:
  Color color_;
};

// A 'filtering' device that adds translucency (specified in the [0-128] range).
typedef ColorFilter<Opaqueness> TranslucencyFilter;

// A 'filtering' device that super-imposes a (usually semi-transparent) overlay
// color.
typedef ColorFilter<Overlay> OverlayFilter;

namespace internal {

struct Disablement {
  Color operator()(Color c, Color bg) const {
    uint8_t a = (c.a() * 32) >> 7;
    uint8_t gray = (((int16_t)c.r() * 3) + ((int16_t)c.g() * 4) + c.b()) >> 3;
    return alphaBlend(bg, Color(a << 24 | gray * 0x010101));
    // return color::Transparent;
  }
};

}  // namespace internal

typedef ColorFilter<internal::Disablement> DisablementFilter;

}  // namespace roo_display

#pragma once

#include "roo_display/color/color.h"
#include "roo_display/core/device.h"

namespace roo_display {

/// Color-only filter that transforms pixels before writing.
///
/// The filter depends only on the color, not position. Examples: translucency
/// adjustments, color transforms, overlays.
template <typename Filter>
class ColorFilter : public DisplayOutput {
 public:
  /// Create a filter with a custom filter functor.
  ColorFilter(DisplayOutput& output, Filter filter,
              Color bgcolor = color::Transparent)
      : output_(output), filter_(filter), bgcolor_(bgcolor) {}

  /// Create a filter using the default-constructed functor.
  ColorFilter(DisplayOutput& output, Color bgcolor = color::Transparent)
      : ColorFilter(output, Filter(), bgcolor) {}

  virtual ~ColorFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode blending_mode) override {
    output_.setAddress(x0, y0, x1, y1, blending_mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    transform(color, pixel_count);
    output_.write(color, pixel_count);
  }

  void writeRects(BlendingMode blending_mode, Color* color, int16_t* x0,
                  int16_t* y0, int16_t* x1, int16_t* y1,
                  uint16_t count) override {
    transform(color, count);
    output_.writeRects(blending_mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode blending_mode, Color color, int16_t* x0,
                 int16_t* y0, int16_t* x1, int16_t* y1,
                 uint16_t count) override {
    output_.fillRects(blending_mode, transform(color), x0, y0, x1, y1, count);
  }

  void writePixels(BlendingMode blending_mode, Color* color, int16_t* x,
                   int16_t* y, uint16_t pixel_count) override {
    transform(color, pixel_count);
    output_.writePixels(blending_mode, color, x, y, pixel_count);
  }

  void fillPixels(BlendingMode blending_mode, Color color, int16_t* x,
                  int16_t* y, uint16_t pixel_count) override {
    output_.fillPixels(blending_mode, transform(color), x, y, pixel_count);
  }

 private:
  void transform(Color* colors, uint16_t count) const {
    for (uint16_t i = 0; i < count; i++) {
      colors[i] = transform(colors[i]);
    }
  }

  // NOTE: in principle, we could apply the filter to 'c', and then alpha-blend
  // the result with bgcolor. It would make the filter interface simpler.
  // Instead, we're pushing bgcolor also to the filter, so that all of that
  // blending is done in the filter. The reason is performance. The foreground
  // color 'c' may often be transparent or translucent. The filter may then
  // combine it with another translucent color (e.g. a translucent overlay), and
  // we would then finally alpha-blend it over the background color (usually
  // opaque):
  //
  //   AlphaBlend(bg, AlphaBlend(c, overlay))
  //
  // But alpha-blending translucencies is more expensive than alpha-blending
  // with opaque. Therefore, it is equivalent but faster, in case of our
  // translucent overlay, to first alpha-blend 'c' over background (which will
  // usually produce an opaque color), and then alpha-blend the overlay over it:
  //
  //   AlphaBlend(AlphaBlend(bg, c), overlay)
  //
  // We push bg to the filter so that it could apply such reorderings if needed.
  Color transform(Color c) const { return filter_(c, bgcolor_); }

  DisplayOutput& output_;
  Filter filter_;
  Color bgcolor_;
};

/// Filter functor that scales alpha (0..127).
class Opaqueness {
 public:
  // Sets opaqueness of the input color. 127 = opaque (no effect). 0 = fully
  // transparent (equivalent to erasure).
  Opaqueness(uint8_t opaqueness) : opaqueness_(opaqueness) {}

  Color operator()(Color c, Color bg) const {
    c.set_a((c.a() * opaqueness_) >> 7);
    return AlphaBlend(bg, c);
  }

 private:
  uint8_t opaqueness_;
};

/// Filter functor that always returns the background color.
class Erasure {
 public:
  // Always writes the bgcolor.
  Erasure() {}

  Color operator()(Color c, Color bg) const { return bg; }
};

/// Specialization for `Erasure` with more efficient operations.
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
                  BlendingMode blending_mode) override {
    output_.setAddress(x0, y0, x1, y1, blending_mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    for (uint16_t i = 0; i < pixel_count; i++) {
      color[i] = bgcolor_;
    }
    output_.write(color, pixel_count);
  }

  void writeRects(BlendingMode blending_mode, Color* color, int16_t* x0,
                  int16_t* y0, int16_t* x1, int16_t* y1,
                  uint16_t count) override {
    output_.fillRects(blending_mode, bgcolor_, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode blending_mode, Color color, int16_t* x0,
                 int16_t* y0, int16_t* x1, int16_t* y1,
                 uint16_t count) override {
    output_.fillRects(blending_mode, bgcolor_, x0, y0, x1, y1, count);
  }

  void writePixels(BlendingMode blending_mode, Color* color, int16_t* x,
                   int16_t* y, uint16_t pixel_count) override {
    output_.fillPixels(blending_mode, bgcolor_, x, y, pixel_count);
  }

  void fillPixels(BlendingMode blending_mode, Color color, int16_t* x,
                  int16_t* y, uint16_t pixel_count) override {
    output_.fillPixels(blending_mode, bgcolor_, x, y, pixel_count);
  }

 private:
  DisplayOutput& output_;
  Color bgcolor_;
};

/// Filter functor that alpha-blends a fixed overlay color.
class Overlay {
 public:
  Overlay(Color color) : color_(color) {}

  Color operator()(Color c, Color bg) const {
    return AlphaBlend(AlphaBlend(bg, c), color_);
  }

 private:
  Color color_;
};

/// Filter that adds translucency (opaqueness in [0, 128]).
typedef ColorFilter<Opaqueness> TranslucencyFilter;

/// Filter that super-imposes a (usually semi-transparent) overlay color.
typedef ColorFilter<Overlay> OverlayFilter;

namespace internal {

struct Disablement {
  Color operator()(Color c, Color bg) const {
    uint8_t a = (c.a() * 32) >> 7;
    uint8_t gray = (((int16_t)c.r() * 3) + ((int16_t)c.g() * 4) + c.b()) >> 3;
    return AlphaBlend(bg, Color(a << 24 | gray * 0x010101));
    // return color::Transparent;
  }
};

}  // namespace internal

/// Filter that applies a disabled/gray appearance.
typedef ColorFilter<internal::Disablement> DisablementFilter;

}  // namespace roo_display

#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

class Synthetic {
 public:
  virtual Box extents() const = 0;
  virtual TransparencyMode transparency() const = 0;
  virtual void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                          Color* result) const = 0;
};

template <typename Getter>
class SimpleSynthetic : public Synthetic, public Drawable {
 public:
  SimpleSynthetic(Box extents, Getter getter = Getter())
      : extents_(extents), getter_(getter) {}

  Box extents() const override { return extents_; }
  void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    while (count-- > 0) {
      *result++ = getter_(*x++, *y++);
    }
  }

  void drawTo(const Surface& s) const override {
    Box clip_box = Box::intersect(s.clip_box, extents_.translate(s.dx, s.dy));
    Box source_bounds = clip_box.translate(-s.dx, -s.dy);
    s.out->setAddress(clip_box);
    BufferedColorWriter writer(s.out, PAINT_MODE_REPLACE);
    for (int16_t y = source_bounds.yMin(); y <= source_bounds.yMax(); ++y) {
      for (int16_t x = source_bounds.xMin(); x <= source_bounds.xMax(); ++x) {
        writer.writeColor(getter_(x, y));
      }
    }
  }

  TransparencyMode transparency() const override { return TRANSPARENCY_NONE; }

 private:
  Box extents_;
  TransparencyMode transparency_;
  Getter getter_;
};

template <typename Getter>
SimpleSynthetic<Getter> MakeSynthetic(Box extents, Getter getter = Getter()) {
  return SimpleSynthetic<Getter>(extents, getter);
}

}  // namespace roo_display
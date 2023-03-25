#pragma once

#include "roo_display.h"
#include "roo_display/color/color.h"

namespace roo_display {

struct FpPoint {
  float x;
  float y;
};

// Anti-aliased wide line with different width at each end and round endings.
class Wedge : public Drawable {
 public:
  Wedge(FpPoint a, float width_a, FpPoint b, float width_b, Color color);

  Box extents() const override { return extents_; }

 private:
  void drawInteriorTo(const Surface& s) const override;

  float ax_;
  float ay_;
  float bx_;
  float by_;
  float aw_;
  float bw_;
  Color color_;
  Box extents_;
};

inline Wedge WedgedLine(FpPoint a, float width_a, FpPoint b, float width_b,
                        Color color) {
  return Wedge(a, width_a, b, width_b, color);
}

inline Wedge ThickLine(FpPoint a, FpPoint b, float width, Color color) {
  return Wedge(a, width, b, width, color);
}

}  // namespace roo_display

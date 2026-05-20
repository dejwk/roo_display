#include "roo_display/shape/impl/smooth_pixel.h"

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {
namespace internal {

void DrawPixel(SmoothShape::Pixel pixel, const Surface& s, const Box& box) {
  int16_t x = box.xMin();
  int16_t y = box.yMin();
  s.out().fillPixels(s.blending_mode(), AlphaBlend(s.bgcolor(), pixel.color),
                     &x, &y, 1);
}

}  // namespace internal
}  // namespace roo_display
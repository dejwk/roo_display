#include "roo_display/shape/smooth.h"

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

namespace {

inline uint8_t getAlpha(float r, float xpax, float ypay, float bax, float bay,
                        float dr) {
  float h = fmaxf(
      fminf((xpax * bax + ypay * bay) / (bax * bax + bay * bay), 1.0f), 0.0f);
  float dx = xpax - bax * h;
  float dy = ypay - bay * h;
  float d = r - sqrtf(dx * dx + dy * dy) - h * dr;
  return (d >= 1.0 ? 255 : d <= 0.0 ? 0 : (uint8_t)(d * 255));
}

}  // namespace

Wedge::Wedge(FpPoint a, float a_width, FpPoint b, float b_width, Color color)
    : ax_(a.x),
      ay_(a.y),
      bx_(b.x),
      by_(b.y),
      aw_(a_width),
      bw_(b_width),
      color_(color) {
  int16_t x0 = (int32_t)floorf(fminf(a.x - a_width, b.x - b_width));
  int16_t y0 = (int32_t)floorf(fminf(a.y - a_width, b.y - b_width));
  int16_t x1 = (int32_t)ceilf(fmaxf(a.x + a_width, b.x + b_width));
  int16_t y1 = (int32_t)ceilf(fmaxf(a.y + a_width, b.y + b_width));
  extents_ = Box(x0, y0, x1, y1);
}

void Wedge::drawInteriorTo(const Surface& s) const {
  float ar = aw_;
  float br = bw_;
  float ax = ax_;
  float ay = ay_;
  float bx = bx_;
  float by = by_;

  // Establish x start and y start.
  int32_t ys = ay;
  if ((ax - ar) > (bx - br)) ys = by;

  float rdt = ar - br;  // Radius delta.
  uint8_t alpha = 255;
  ar += 0.5;

  float xpax, ypay, bax = bx - ax, bay = by - ay;

  Box box = Box::Intersect(extents_, s.clip_box().translate(-s.dx(), -s.dy()));
  if (box.empty()) {
    return;
  }
  if (ys < box.yMin()) ys = box.yMin();
  int32_t xs = box.xMin();

  BufferedPixelWriter writer(s.out(), s.paint_mode());
  int16_t dx = s.dx();
  int16_t dy = s.dy();

  // Scan bounding box from ys down, calculate pixel intensity from distance to
  // line.
  for (int32_t yp = ys; yp <= box.yMax(); yp++) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0) break;  // Skip right side.
      xpax = xp - ax;
      alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt);
      if (alpha == 0) continue;
      // Track edge to minimise calculations.
      if (!endX) {
        endX = true;
        xs = xp;
      }
      writer.writePixel(xp + dx, yp + dy, color_.withA(alpha));
    }
  }

  // Reset x start to left side of box.
  xs = box.xMin();
  // Scan bounding box from ys-1 up, calculate pixel intensity from distance to
  // line.
  for (int32_t yp = ys - 1; yp >= box.yMin(); yp--) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0) break;  // Skip right side of drawn line.
      xpax = xp - ax;
      alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt);
      if (alpha == 0) continue;
      // Track line boundary.
      if (!endX) {
        endX = true;
        xs = xp;
      }
      writer.writePixel(xp + dx, yp + dy, color_.withA(alpha));
    }
  }
}

}  // namespace roo_display

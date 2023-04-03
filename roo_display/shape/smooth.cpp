#include "roo_display/shape/smooth.h"

#include <Arduino.h>
#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

namespace {

inline uint8_t getAlpha(float r, float xpax, float ypay, float bax, float bay,
                        float dr, uint8_t max_alpha) {
  float h = fmaxf(
      fminf((xpax * bax + ypay * bay) / (bax * bax + bay * bay), 1.0f), 0.0f);
  float dx = xpax - bax * h;
  float dy = ypay - bay * h;
  float d = r - sqrtf(dx * dx + dy * dy) - h * dr;
  return (d >= 1.0 ? max_alpha : d <= 0.0 ? 0 : (uint8_t)(d * max_alpha));
}

}  // namespace

SmoothWedgeShape::SmoothWedgeShape(FpPoint a, float a_width, FpPoint b,
                                   float b_width, Color color)
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

void SmoothWedgeShape::drawInteriorTo(const Surface& s) const {
  float ar = aw_;
  float br = bw_;
  float ax = ax_;
  float ay = ay_;
  float bx = bx_;
  float by = by_;
  uint8_t max_alpha = color_.a();

  // Establish x start and y start.
  int32_t ys = ay;
  if ((ax - ar) > (bx - br)) ys = by;

  float rdt = ar - br;  // Radius delta.
  uint8_t alpha = max_alpha;
  ar += 0.5f;

  float xpax, ypay, bax = bx - ax, bay = by - ay;

  Box box = Box::Intersect(extents_, s.clip_box().translate(-s.dx(), -s.dy()));
  if (box.empty()) {
    return;
  }
  if (ys < box.yMin()) ys = box.yMin();
  int32_t xs = box.xMin();
  int16_t dx = s.dx();
  int16_t dy = s.dy();
  Color preblended = AlphaBlend(s.bgcolor(), color_);

  BufferedPixelWriter writer(s.out(), s.paint_mode());

  // Scan bounding box from ys down, calculate pixel intensity from distance to
  // line.
  for (int32_t yp = ys; yp <= box.yMax(); yp++) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0) break;  // Skip right side.
      xpax = xp - ax;
      alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
      if (alpha == 0) continue;
      // Track the edge to minimize calculations.
      if (!endX) {
        endX = true;
        xs = xp;
      }
      writer.writePixel(xp + dx, yp + dy,
                        alpha == max_alpha
                            ? preblended
                            : AlphaBlend(s.bgcolor(), color_.withA(alpha)));
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
      alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
      if (alpha == 0) continue;
      // Track line boundary.
      if (!endX) {
        endX = true;
        xs = xp;
      }
      writer.writePixel(xp + dx, yp + dy,
                        alpha == max_alpha
                            ? preblended
                            : AlphaBlend(s.bgcolor(), color_.withA(alpha)));
    }
  }
}

SmoothRoundRectShape::SmoothRoundRectShape(float x0, float y0, float x1,
                                           float y1, float radius, Color color)
    : x0_(x0), y0_(y0), x1_(x1), y1_(y1), r_(radius), color_(color) {
  {
    int16_t xMin = (int16_t)roundf(x0_ - r_);
    int16_t yMin = (int16_t)roundf(y0_ - r_);
    int16_t xMax = (int16_t)roundf(x1_ + r_);
    int16_t yMax = (int16_t)roundf(y1_ + r_);
    extents_ = Box(xMin, yMin, xMax, yMax);
  }
  {
    int16_t xMin = (int16_t)ceilf(x0_ - r_ + 0.5f);
    int16_t xMax = (int16_t)floorf(x1_ + r_ - 0.5f);
    int16_t yMin = (int16_t)ceilf(y0_ + 0.5f);
    int16_t yMax = (int16_t)floorf(y1_ - 0.5f);
    inner_wide_ = Box(xMin, yMin, xMax, yMax);
  }
  {
    int16_t xMin = (int16_t)ceilf(x0_ + 0.5f);
    int16_t xMax = (int16_t)floorf(x1_ - 0.5f);
    int16_t yMin = (int16_t)ceilf(y0_ - r_ + 0.5f);
    int16_t yMax = (int16_t)floorf(y1_ + r_ - 0.5f);
    inner_tall_ = Box(xMin, yMin, xMax, yMax);
  }
  {
    float d = sqrtf(0.5) * r_;
    int16_t xMin = (int16_t)ceilf(x0_ - d + 0.5f);
    int16_t xMax = (int16_t)floorf(x1_ + d - 0.5f);
    int16_t yMin = (int16_t)ceilf(y0_ - d + 0.5f);
    int16_t yMax = (int16_t)floorf(y1_ + d - 0.5f);
    inner_mid_ = Box(xMin, yMin, xMax, yMax);
  }
  tmax_ = (int16_t)floor((1.0 - sqrtf((2 - sqrt(2))) / 2) * (r_ - 0.5f)) - 1;
}

void SmoothRoundRectShape::readColors(const int16_t* x, const int16_t* y,
                                      uint32_t count, Color* result) const {
  uint8_t max_alpha = color_.a();
  while (count-- > 0) {
    *result++ = color_.withA(getAlpha(*x++, *y++, max_alpha));
  }
}

bool SmoothRoundRectShape::readColorRect(int16_t xMin, int16_t yMin,
                                         int16_t xMax, int16_t yMax,
                                         Color* result) const {
  Box box(xMin, yMin, xMax, yMax);
  if (inner_mid_.contains(box) || inner_wide_.contains(box) ||
      inner_tall_.contains(box)) {
    *result = color_;
    return true;
  }
  if (xMax - extents_.xMin() + yMax - extents_.yMin() < tmax_) {
    *result = color::Transparent;
    return true;
  }
  if (extents_.xMax() - xMin + yMax - extents_.yMin() < tmax_) {
    *result = color::Transparent;
    return true;
  }
  if (xMax - extents_.xMin() + extents_.yMax() - yMin < tmax_) {
    *result = color::Transparent;
    return true;
  }
  if (extents_.xMax() - xMin + extents_.yMax() - yMin < tmax_) {
    *result = color::Transparent;
    return true;
  }

  uint8_t max_alpha = color_.a();
  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = color_.withA(getAlpha(x, y, max_alpha));
    }
  }
  uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
  Color c = result[0];
  for (uint32_t i = 1; i < pixel_count; i++) {
    if (result[i] != c) return false;
  }
  return true;
}

uint8_t SmoothRoundRectShape::getAlpha(int16_t x, int16_t y,
                                       uint8_t max_alpha) const {
  if (inner_wide_.contains(x, y)) {
    return max_alpha;
  }
  if (inner_tall_.contains(x, y)) {
    return max_alpha;
  }
  if (inner_mid_.contains(x, y)) {
    return max_alpha;
  }
  float ref_x = x;
  if (ref_x < x0_) ref_x = x0_;
  if (ref_x > x1_) ref_x = x1_;
  float ref_y = y;
  if (ref_y < y0_) ref_y = y0_;
  if (ref_y > y1_) ref_y = y1_;
  float dx = x - ref_x;
  float dy = y - ref_y;
  // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < r_ - 0.5) {
  //   *result++ = color_;
  //   continue;
  // }

  float d_squared = dx * dx + dy * dy;
  float r_squared_adjusted = r_ * r_ + 0.25f;
  if (d_squared <= r_squared_adjusted - r_) {
    return max_alpha;
  }
  if (d_squared >= r_squared_adjusted + r_) {
    return 0;
  }
  float d = sqrtf(d_squared);
  return (uint8_t)(max_alpha * (r_ - d + 0.5f));
}

namespace {

struct SmoothRoundRectSpec {
  DisplayOutput* out;
  FillMode fill_mode;
  PaintMode paint_mode;
  float x0;
  float y0;
  float x1;
  float y1;
  float r;
  float r_squared_adjusted;
  Color color;
  Color bgcolor;
  Color pre_blended;
  Box inner_wide;
  Box inner_mid;
  Box inner_tall;
};

uint8_t GetSmoothRoundRectShapeAlpha(SmoothRoundRectSpec& spec, int16_t x,
                                     int16_t y) {
  if (spec.inner_wide.contains(x, y)) {
    return spec.color.a();
  }
  if (spec.inner_tall.contains(x, y)) {
    return spec.color.a();
  }
  if (spec.inner_mid.contains(x, y)) {
    return spec.color.a();
  }
  float ref_x = x;
  if (ref_x < spec.x0) ref_x = spec.x0;
  if (ref_x > spec.x1) ref_x = spec.x1;
  float ref_y = y;
  if (ref_y < spec.y0) ref_y = spec.y0;
  if (ref_y > spec.y1) ref_y = spec.y1;
  float dx = x - ref_x;
  float dy = y - ref_y;
  // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < r_ - 0.5) {
  //   *result++ = color_;
  //   continue;
  // }

  float d_squared = dx * dx + dy * dy;
  if (d_squared <= spec.r_squared_adjusted - spec.r) {
    return spec.color.a();
  }
  if (d_squared >= spec.r_squared_adjusted + spec.r) {
    return 0;
  }
  float d = sqrtf(d_squared);
  return (uint8_t)(spec.color.a() * (spec.r - d + 0.5f));
}

// Called for rectangles with area <= 64 pixels.
void FillSmoothRoundRectRectInternal(SmoothRoundRectSpec& spec, int16_t xMin,
                                     int16_t yMin, int16_t xMax, int16_t yMax) {
  Box box(xMin, yMin, xMax, yMax);
  if (spec.inner_mid.contains(box) || spec.inner_wide.contains(box) ||
      spec.inner_tall.contains(box)) {
    spec.out->fillRect(spec.paint_mode, box, spec.pre_blended);
    return;
  }
  if (xMax < spec.x0) {
    if (yMax < spec.y0) {
      float dx = (spec.x0 - xMax);
      float dy = (spec.y0 - yMax);
      if (dx * dx + dy * dy > spec.r * spec.r + 1) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    } else if (yMin > spec.y1) {
      float dx = (spec.x0 - xMax);
      float dy = (spec.y1 - yMin);
      if (dx * dx + dy * dy > spec.r * spec.r + 1) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    }
  } else if (xMin > spec.x1) {
    if (yMax < spec.y0) {
      float dx = (spec.x1 - xMin);
      float dy = (spec.y0 - yMax);
      if (dx * dx + dy * dy > spec.r * spec.r + 1) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    } else if (yMin > spec.y1) {
      float dx = (spec.x1 - xMin);
      float dy = (spec.y1 - yMin);
      if (dx * dx + dy * dy > spec.r * spec.r + 1) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    }
  }
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
    BufferedPixelWriter writer(*spec.out, spec.paint_mode);
    for (int16_t y = yMin; y <= yMax; ++y) {
      for (int16_t x = xMin; x <= xMax; ++x) {
        uint8_t a = GetSmoothRoundRectShapeAlpha(spec, x, y);
        if (a == 0) continue;
        writer.writePixel(x, y,
                          a == spec.color.a()
                              ? spec.pre_blended
                              : AlphaBlend(spec.bgcolor, spec.color.withA(a)));
      }
    }
  } else {
    Color color[64];
    int cnt = 0;
    for (int16_t y = yMin; y <= yMax; ++y) {
      for (int16_t x = xMin; x <= xMax; ++x) {
        uint8_t a = GetSmoothRoundRectShapeAlpha(spec, x, y);
        color[cnt++] = a == 0 ? spec.bgcolor
                       : a == spec.color.a()
                           ? spec.pre_blended
                           : AlphaBlend(spec.bgcolor, spec.color.withA(a));
      }
    }
    spec.out->setAddress(Box(xMin, yMin, xMax, yMax), spec.paint_mode);
    spec.out->write(color, cnt);
  }
}

}  // namespace

void SmoothRoundRectShape::drawTo(const Surface& s) const {
  Box box = Box::Intersect(extents_.translate(s.dx(), s.dy()), s.clip_box());
  if (box.empty()) return;
  SmoothRoundRectSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .paint_mode = s.paint_mode(),
      .x0 = x0_,
      .y0 = y0_,
      .x1 = x1_,
      .y1 = y1_,
      .r = r_,
      .r_squared_adjusted = r_ * r_ + 0.25f,
      .color = color_,
      .bgcolor = s.bgcolor(),
      .pre_blended = AlphaBlend(s.bgcolor(), color_),
      .inner_wide = inner_wide_,
      .inner_mid = inner_mid_,
      .inner_tall = inner_tall_,
  };
  if (s.dx() != 0 || s.dy() != 0) {
    spec.x0 += s.dx();
    spec.y0 += s.dy();
    spec.x1 += s.dx();
    spec.y1 += s.dy();
    spec.inner_wide = spec.inner_wide.translate(s.dx(), s.dy());
    spec.inner_mid = spec.inner_mid.translate(s.dx(), s.dy());
    spec.inner_tall = spec.inner_tall.translate(s.dx(), s.dy());
  }
  int16_t xMin = box.xMin();
  int16_t xMax = box.xMax();
  int16_t yMin = box.yMin();
  int16_t yMax = box.yMax();
  {
    uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
    if (pixel_count <= 64) {
      FillSmoothRoundRectRectInternal(spec, xMin, yMin, xMax, yMax);
      return;
    }
  }
  const int16_t xMinOuter = (xMin / 8) * 8;
  const int16_t yMinOuter = (yMin / 8) * 8;
  const int16_t xMaxOuter = (xMax / 8) * 8 + 7;
  const int16_t yMaxOuter = (yMax / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSmoothRoundRectRectInternal(
          spec, std::max(x, xMin), std::max(y, yMin),
          std::min((int16_t)(x + 7), xMax), std::min((int16_t)(y + 7), yMax));
    }
  }
}

}  // namespace roo_display

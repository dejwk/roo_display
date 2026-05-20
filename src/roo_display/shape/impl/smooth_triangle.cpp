#include "roo_display/shape/impl/smooth_triangle.h"

#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

SmoothShape SmoothFilledTriangle(FpPoint a, FpPoint b, FpPoint c, Color color) {
  int16_t xMin = floorf(std::min(std::min(a.x, b.x), c.x));
  int16_t yMin = floorf(std::min(std::min(a.y, b.y), c.y));
  int16_t xMax = ceilf(std::max(std::max(a.x, b.x), c.x));
  int16_t yMax = ceilf(std::max(std::max(a.y, b.y), c.y));
  float d12 = sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
  float d23 = sqrtf((c.x - b.x) * (c.x - b.x) + (c.y - b.y) * (c.y - b.y));
  float d31 = sqrtf((a.x - c.x) * (a.x - c.x) + (a.y - c.y) * (a.y - c.y));
  return SmoothShape(
      Box(xMin, yMin, xMax, yMax),
      SmoothShape::Triangle{a.x, a.y, (b.x - a.x) / d12, (b.y - a.y) / d12, b.x,
                            b.y, (c.x - b.x) / d23, (c.y - b.y) / d23, c.x, c.y,
                            (a.x - c.x) / d31, (a.y - c.y) / d31, color});
};

namespace {

struct TriangleDrawSpec {
  DisplayOutput* out;
  FillMode fill_mode;
  BlendingMode blending_mode;
  Color bgcolor;
  Color pre_blended_interior;
};

Color GetSmoothTrianglePixelColor(const SmoothShape::Triangle& t, int16_t x,
                                  int16_t y) {
  float n1 = t.dy12 * (x - t.x1) - t.dx12 * (y - t.y1);
  float n2 = t.dy23 * (x - t.x2) - t.dx23 * (y - t.y2);
  float n3 = t.dy31 * (x - t.x3) - t.dx31 * (y - t.y3);
  if (n1 <= -0.5f && n2 <= -0.5f && n3 <= -0.5f) {
    // Fully inside the triangle.
    return t.color;
  }
  if (n1 >= 0.5f || n2 >= 0.5f || n3 >= 0.5f) {
    // Fully outside the triangle.
    return color::Transparent;
  }
  // Somewhere near the boundery of the triangle; perhaps near more than one.
  // NOTE: this formula isn't accurate for very thin triangles, producing
  // somewhat jittery lines. For now, prefer wedge in such cases.
  return t.color.withA(roundf(t.color.a() * std::min(1.0f, 0.5f - n1) *
                              std::min(1.0f, 0.5f - n2) *
                              std::min(1.0f, 0.5f - n3)));
}

inline bool IsPointWithinTriangle(const SmoothShape::Triangle& t, float x,
                                  float y) {
  float n1 = t.dy12 * (x - t.x1) - t.dx12 * (y - t.y1);
  if (n1 > -0.5f) return false;
  float n2 = t.dy23 * (x - t.x2) - t.dx23 * (y - t.y2);
  if (n2 > -0.5f) return false;
  float n3 = t.dy31 * (x - t.x3) - t.dx31 * (y - t.y3);
  if (n3 > -0.5f) return false;
  return true;
}

inline bool IsRectWithinTriangle(const SmoothShape::Triangle& t,
                                 const Box& box) {
  return IsPointWithinTriangle(t, box.xMin(), box.yMin()) &&
         IsPointWithinTriangle(t, box.xMin(), box.yMax()) &&
         IsPointWithinTriangle(t, box.xMax(), box.yMin()) &&
         IsPointWithinTriangle(t, box.xMax(), box.yMax());
}

inline bool IsRectOutsideLine(float x0, float y0, float dx, float dy,
                              const Box& box) {
  float n1 = dy * (box.xMin() - x0) - dx * (box.yMin() - y0);
  if (n1 <= 0.5f) return false;
  float n2 = dy * (box.xMin() - x0) - dx * (box.yMax() - y0);
  if (n2 <= 0.5f) return false;
  float n3 = dy * (box.xMax() - x0) - dx * (box.yMin() - y0);
  if (n3 <= 0.5f) return false;
  float n4 = dy * (box.xMax() - x0) - dx * (box.yMax() - y0);
  if (n4 <= 0.5f) return false;
  return true;
}

inline bool IsRectOutsideTriangle(const SmoothShape::Triangle& t,
                                  const Box& box) {
  // It is not enough to check that all points are outside triangle; we must
  // check that they fall on the same side.
  return IsRectOutsideLine(t.x1, t.y1, t.dx12, t.dy12, box) ||
         IsRectOutsideLine(t.x2, t.y2, t.dx23, t.dy23, box) ||
         IsRectOutsideLine(t.x3, t.y3, t.dx31, t.dy31, box);
}

// Called for rects with area <= 64 pixels.
internal::RectColor DetermineRectColorForTriangleImpl(
    const SmoothShape::Triangle& triangle, const Box& box) {
  // First, let's see if the rect is entirely within the triangle.
  if (IsRectWithinTriangle(triangle, box)) {
    return internal::INTERIOR;
  }
  if (IsRectOutsideTriangle(triangle, box)) {
    return internal::TRANSPARENT;
  }
  return internal::NON_UNIFORM;
}

// Called for arcs with area <= 64 pixels.
void FillSubrectOfTriangle(const SmoothShape::Triangle& triangle,
                           const TriangleDrawSpec& spec, const Box& box) {
  Color interior = triangle.color;
  switch (DetermineRectColorForTriangleImpl(triangle, box)) {
    case internal::TRANSPARENT: {
      if (spec.fill_mode == FillMode::kExtents) {
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case internal::INTERIOR: {
      if (spec.fill_mode == FillMode::kExtents ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FillMode::kVisible) {
    BufferedPixelWriter writer(*spec.out, spec.blending_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothTrianglePixelColor(triangle, x, y);
        if (c == color::Transparent) continue;
        writer.writePixel(x, y,
                          c == interior ? spec.pre_blended_interior
                                        : AlphaBlend(spec.bgcolor, c));
      }
    }
  } else {
    Color color[64];
    int cnt = 0;
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothTrianglePixelColor(triangle, x, y);
        color[cnt++] = c.a() == 0      ? spec.bgcolor
                       : c == interior ? spec.pre_blended_interior
                                       : AlphaBlend(spec.bgcolor, c);
      }
    }
    spec.out->setAddress(box, spec.blending_mode);
    spec.out->write(color, cnt);
  }
}

void DrawTriangleImpl(SmoothShape::Triangle triangle, const Surface& s,
                      const Box& box) {
  TriangleDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .blending_mode = s.blending_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), triangle.color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    triangle.x1 += s.dx();
    triangle.y1 += s.dx();
    triangle.x2 += s.dx();
    triangle.y2 += s.dx();
    triangle.x3 += s.dx();
    triangle.y3 += s.dx();
  }
  int16_t xMin = box.xMin();
  int16_t xMax = box.xMax();
  int16_t yMin = box.yMin();
  int16_t yMax = box.yMax();
  {
    uint32_t pixel_count = box.area();
    if (pixel_count <= 64) {
      FillSubrectOfTriangle(triangle, spec, box);
      return;
    }
  }
  const int16_t xMinOuter = (xMin / 8) * 8;
  const int16_t yMinOuter = (yMin / 8) * 8;
  const int16_t xMaxOuter = (xMax / 8) * 8 + 7;
  const int16_t yMaxOuter = (yMax / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSubrectOfTriangle(
          triangle, spec,
          Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
              std::min((int16_t)(x + 7), box.xMax()),
              std::min((int16_t)(y + 7), box.yMax())));
    }
  }
}

}  // namespace

namespace internal {

RectColor DetermineRectColorForTriangle(const SmoothShape::Triangle& triangle,
                                        const Box& box) {
  return DetermineRectColorForTriangleImpl(triangle, box);
}

bool ReadColorRectOfTriangle(const SmoothShape::Triangle& triangle,
                             int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForTriangleImpl(triangle, box)) {
    case TRANSPARENT: {
      *result = color::Transparent;
      return true;
    }
    case INTERIOR: {
      *result = triangle.color;
      return true;
    }
    default:
      break;
  }
  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = GetSmoothTrianglePixelColor(triangle, x, y);
    }
  }
  // // // This is now very unlikely to be true, or we would have caught it
  // above. Color c = result[0]; uint32_t pixel_count = box.area(); for
  // (uint32_t i = 1; i < pixel_count; i++) {
  //   if (result[i] != c) return false;
  // }
  // return true;
  return false;
}

void ReadTriangleColors(const SmoothShape::Triangle& triangle, const int16_t* x,
                        const int16_t* y, uint32_t count, Color* result) {
  while (count-- > 0) {
    *result++ = GetSmoothTrianglePixelColor(triangle, *x++, *y++);
  }
}

void DrawTriangle(SmoothShape::Triangle triangle, const Surface& s,
                  const Box& box) {
  DrawTriangleImpl(triangle, s, box);
}

}  // namespace internal

}  // namespace roo_display
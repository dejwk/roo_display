#include "roo_display/shape/smooth.h"

#include <Arduino.h>
#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

SmoothShape::SmoothShape(Box extents, Wedge wedge)
    : kind_(SmoothShape::WEDGE),
      extents_(std::move(extents)),
      wedge_(std::move(wedge)) {}

SmoothShape::SmoothShape(Box extents, RoundRect round_rect)
    : kind_(SmoothShape::ROUND_RECT),
      extents_(std::move(extents)),
      round_rect_(std::move(round_rect)) {}

SmoothShape::SmoothShape()
    : kind_(SmoothShape::EMPTY), extents_(0, 0, -1, -1) {}

SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b, float width_b,
                             Color color, EndingStyle ending_style) {
  if (width_a < 0.0f) width_a = 0.0f;
  if (width_b < 0.0f) width_b = 0.0f;
  float ar = width_a / 2.0f;
  float br = width_b / 2.0f;
  if (ending_style == ENDING_ROUNDED) {
    float dr = sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    if (ar + dr <= br) {
      // 'a' fits in 'b'.
      return SmoothFilledCircle({b.x, b.y}, br, color);
    } else if (br + dr <= ar) {
      // 'b' fits in 'a'.
      return SmoothFilledCircle({a.x, a.y}, ar, color);
    }
  }
  if (a.x == b.x && a.y == b.y) {
    // Must be ENDING_FLAT. Nothing to draw.
    return SmoothShape();
  }

  SmoothShape::Wedge wedge{a.x, a.y, b.x,   b.y,
                           ar,  br,  color, (ending_style == ENDING_ROUNDED)};
  int16_t x0 = (int32_t)floorf(fminf(a.x - ar, b.x - br));
  int16_t y0 = (int32_t)floorf(fminf(a.y - ar, b.y - br));
  int16_t x1 = (int32_t)ceilf(fmaxf(a.x + ar, b.x + br));
  int16_t y1 = (int32_t)ceilf(fmaxf(a.y + ar, b.y + br));
  return SmoothShape(Box(x0, y0, x1, y1), std::move(wedge));
}

SmoothShape SmoothThickLine(FpPoint a, FpPoint b, float width, Color color,
                            EndingStyle ending_style) {
  return SmoothWedgedLine(a, width, b, width, color, ending_style);
}

SmoothShape SmoothLine(FpPoint a, FpPoint b, Color color) {
  return SmoothThickLine(a, b, 1.0f, color, ENDING_ROUNDED);
}

SmoothShape SmoothRotatedFilledRect(FpPoint center, float width, float height,
                                    float angle, Color color) {
  if (width <= 0.0f || height <= 0.0f) return SmoothShape();
  // We will use width as thickness.
  // Before rotation, and adjusted by the center point, the line endpoints have
  // coordinates (0, height/2) and (0, -height/2).
  float dx = sinf(angle) * (height * 0.5f);
  float dy = cosf(angle) * (height * 0.5f);
  return SmoothThickLine({center.x + dx, center.y + dy},
                         {center.x - dx, center.y - dy}, width, color,
                         ENDING_FLAT);
}

SmoothShape SmoothOutlinedRoundRect(float x0, float y0, float x1, float y1,
                                    float radius, float outline_thickness,
                                    Color outline_color, Color interior_color) {
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);
  float w = x1 - x0;
  float h = y1 - y0;
  float max_radius = ((w < h) ? w : h) / 2;  // 1/2 minor axis.
  if (radius > max_radius) radius = max_radius;
  if (radius < 0) radius = 0;
  if (outline_thickness < 0) outline_thickness = 0;
  float interior_radius = radius - outline_thickness;
  if (interior_radius < 0) interior_radius = 0;
  x0 += radius;
  y0 += radius;
  x1 -= radius;
  y1 -= radius;

  float r = radius;
  float ri = interior_radius;
  float d = sqrtf(0.5f) * interior_radius;
  Box extents((int16_t)roundf(x0 - r), (int16_t)roundf(y0 - r),
              (int16_t)roundf(x1 + r), (int16_t)roundf(y1 + r));
  Box inner_mid((int16_t)ceilf(x0 - d + 0.5f), (int16_t)floorf(x1 + d - 0.5f),
                (int16_t)ceilf(y0 - d + 0.5f), (int16_t)floorf(y1 + d - 0.5f));
  Box inner_wide((int16_t)ceilf(x0 - ri + 0.5f),
                 (int16_t)floorf(x1 + ri - 0.5f), (int16_t)ceilf(y0 + 0.5f),
                 (int16_t)floorf(y1 - 0.5f));
  Box inner_tall((int16_t)ceilf(x0 + 0.5f), (int16_t)floorf(x1 - 0.5f),
                 (int16_t)ceilf(y0 - ri + 0.5f),
                 (int16_t)floorf(y1 + ri - 0.5f));
  SmoothShape::RoundRect spec{x0,
                              y0,
                              x1,
                              y1,
                              radius,
                              interior_radius,
                              outline_color,
                              interior_color,
                              std::move(inner_mid),
                              std::move(inner_wide),
                              std::move(inner_tall)};

  return SmoothShape(std::move(extents), std::move(spec));
}

SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                                  float radius, Color color) {
  return SmoothOutlinedRoundRect(x0, y0, x1, y1, radius, 0, color, color);
}

SmoothShape SmoothOutlinedCircle(FpPoint center, float radius,
                                 float outline_thickness, Color outline_color,
                                 Color interior_color) {
  return SmoothOutlinedRoundRect(center.x - radius, center.y - radius,
                                 center.x + radius, center.y + radius, radius,
                                 outline_thickness, outline_color,
                                 interior_color);
}

SmoothShape SmoothFilledCircle(FpPoint center, float radius, Color color) {
  return SmoothOutlinedCircle(center, radius, 0, color, color);
}

// Helper functions for wedge.

namespace {

struct WedgeDrawSpec {
  float r;
  float dr;
  float bax;
  float bay;
  float hd;
  float sqrt_hd;
  uint8_t max_alpha;
  bool round_endings;
};

inline uint8_t GetWedgeShapeAlpha(const WedgeDrawSpec& spec, float xpax,
                                  float ypay) {
  // hn (h-numerator) is zero at 'a' and equal to spec.hd at 'b'.
  float hn = (xpax * spec.bax + ypay * spec.bay);
  // h is hn/hd trimmed to [0, 1]. It represents the position of the point along
  // the line, with 0 at 'a' and 1 at 'b', that is closest to the point under
  // test. We will call that point 'P'.
  float h = hn < 0.0f ? 0.0f : hn > spec.hd ? 1.0f : hn / spec.hd;
  // (dx, dy) is a distance vector (from point under test to 'P').
  float dx = xpax - spec.bax * h;
  float dy = ypay - spec.bay * h;
  float l_sq = dx * dx + dy * dy;
  // adj_dist says how far the (widened) boundary of our shape is from 'P'.
  float adj_dist = spec.r - h * spec.dr;
  float adj_dist_sq = adj_dist * adj_dist;
  // Check if point under test is outside the (widened) boundary.
  if (adj_dist_sq < l_sq) return 0;
  if (!spec.round_endings) {
    // We now additionally require that the non-trimmed 'h' is not outside [0,
    // 1] by more than a pixel distance (and if it is within a pixel distance,
    // we anti-alias it).
    float d1 = (hn / spec.hd) * spec.sqrt_hd;
    float d2 = (1.0f - hn / spec.hd) * spec.sqrt_hd;

    if (d1 < .5f) {
      if (d1 < -0.5f) return 0;
      float d = adj_dist - sqrtf(l_sq);
      return uint8_t(std::min(d, (d1 + 0.5f)) * spec.max_alpha);
    }
    if (d2 < .5f) {
      if (d2 < -0.5f) return 0;
      float d = adj_dist - sqrtf(l_sq);
      return uint8_t(std::min(d, (d2 + 0.5f)) * spec.max_alpha);
    }
  }
  // Handle sub-pixel widths.
  if (adj_dist < 1.0f) {
    float l = sqrtf(l_sq);
    float d;
    if (l + adj_dist < 1.0f) {
      // Line fully within the pixel. Use line thickness as alpha.
      d = 2 * adj_dist - 1.0f;
    } else {
      // Line on the boundary of the pixel. Use the regular formula.
      d = adj_dist - l;
    }
    return (uint8_t)(d * spec.max_alpha);
  }
  // Equivalent to sqrt(l_sq) < adj_dist - 1 (the point is deep inside the
  // (widened) boundary), but without sqrt.
  if (adj_dist_sq - 2 * adj_dist + 1 > l_sq) return spec.max_alpha;
  // d is the distance of tested point, inside the (widened) boundary, from the
  // (widened) boundary. We now know it is in the [0-1] range.
  float l = sqrtf(l_sq);
  float d = adj_dist - l;
  return (uint8_t)(d * spec.max_alpha);
}

// Helper functions for round rect.

Color GetSmoothRoundRectColorPreChecked(const SmoothShape::RoundRect& spec,
                                        int16_t x, int16_t y) {
  float ref_x = std::min(std::max((float)x, spec.x0), spec.x1);
  float ref_y = std::min(std::max((float)y, spec.y0), spec.y1);
  float dx = x - ref_x;
  float dy = y - ref_y;
  // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < r_ - 0.5) {
  //   *result++ = color_;
  //   continue;
  // }
  float d_squared = dx * dx + dy * dy;
  float ri_squared_adjusted = spec.ri * spec.ri + 0.25f;
  if (d_squared <= ri_squared_adjusted - spec.ri - 1) {
    return spec.interior_color;
  }
  float r_squared_adjusted = spec.r * spec.r + 0.25f;
  if (d_squared >= r_squared_adjusted + spec.r) {
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= r_squared_adjusted - spec.r;
  bool fully_outside_inner =
      spec.r == spec.ri || d_squared >= ri_squared_adjusted + spec.ri;
  if (fully_within_outer && fully_outside_inner) {
    return spec.outline_color;
  }
  float d = sqrtf(d_squared);
  if (fully_outside_inner) {
    return spec.outline_color.withA(
        (uint8_t)(spec.outline_color.a() * (spec.r - d + 0.5f)));
  }
  if (fully_within_outer) {
    return AlphaBlend(
        spec.interior_color,
        spec.outline_color.withA(
            (uint8_t)(spec.outline_color.a() * (1 - (spec.ri - d + 0.5f)))));
  }
  return AlphaBlend(spec.interior_color,
                    spec.outline_color.withA(
                        (uint8_t)(spec.outline_color.a() *
                                  std::max(0.0f, (spec.r - d + 0.5f) -
                                                     (spec.ri - d + 0.5f)))));
}

Color GetSmoothRoundRectColor(const SmoothShape::RoundRect& spec, int16_t x,
                              int16_t y) {
  if (spec.inner_mid.contains(x, y) || spec.inner_wide.contains(x, y) ||
      spec.inner_tall.contains(x, y)) {
    return spec.interior_color;
  }
  return GetSmoothRoundRectColorPreChecked(spec, x, y);
}

struct RoundRectDrawSpec {
  SmoothShape::RoundRect rect;
  DisplayOutput* out;
  FillMode fill_mode;
  PaintMode paint_mode;
  float r_squared_adjusted;
  float ri_squared_adjusted;
  Color bgcolor;
  Color pre_blended_outline;
  Color pre_blended_interior;
};

inline Color GetSmoothRoundRectShapeColor(const RoundRectDrawSpec& spec,
                                          int16_t x, int16_t y) {
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (spec.inner_mid.contains(x, y) || spec.inner_wide.contains(x, y) ||
  //     spec.inner_tall.contains(x, y)) {
  //   return spec.interior;
  // }
  float ref_x = std::min(std::max((float)x, spec.rect.x0), spec.rect.x1);
  float ref_y = std::min(std::max((float)y, spec.rect.y0), spec.rect.y1);
  float dx = x - ref_x;
  float dy = y - ref_y;
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < spec.ri - 0.5) {
  //   return spec.interior;
  // }

  float d_squared = dx * dx + dy * dy;
  float r = spec.rect.r;
  float ri = spec.rect.ri;
  Color interior = spec.rect.interior_color;
  Color outline = spec.rect.outline_color;

  if (d_squared <= spec.ri_squared_adjusted - ri) {
    return interior;
  }
  if (d_squared >= spec.r_squared_adjusted + r) {
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= spec.r_squared_adjusted - r;
  bool fully_outside_inner =
      r == ri || d_squared >= spec.ri_squared_adjusted + ri;
  if (fully_within_outer && fully_outside_inner) {
    return outline;
  }
  // Note: replacing with integer sqrt (iterative, loop-unrolled, 24-bit) slows
  // things down. At 64-bit not loop-unrolled, does so dramatically.
  float d = sqrtf(d_squared);
  if (fully_outside_inner) {
    return outline.withA((uint8_t)(outline.a() * (r - d + 0.5f)));
  }
  if (fully_within_outer) {
    return AlphaBlend(
        interior,
        outline.withA((uint8_t)(outline.a() * (1.0f - (ri - d + 0.5f)))));
  }
  return AlphaBlend(
      interior, outline.withA((uint8_t)(outline.a() *
                                        std::max(0.0f, (r - d + 0.5f) -
                                                           (ri - d + 0.5f)))));
}

float CalcRoundRectDistSq(const SmoothShape::RoundRect& spec, int16_t x,
                          int16_t y) {
  float ref_x = std::min(std::max((float)x, spec.x0), spec.x1);
  float ref_y = std::min(std::max((float)y, spec.y0), spec.y1);
  float dx = x - ref_x;
  float dy = y - ref_y;
  return dx * dx + dy * dy;
}

// Called for rectangles with area <= 64 pixels.
void FillSmoothRoundRectRectInternal(RoundRectDrawSpec& spec, int16_t xMin,
                                     int16_t yMin, int16_t xMax, int16_t yMax) {
  Box box(xMin, yMin, xMax, yMax);
  if (spec.rect.inner_mid.contains(box) || spec.rect.inner_wide.contains(box) ||
      spec.rect.inner_tall.contains(box)) {
    spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_interior);
    return;
  }
  float dtl = CalcRoundRectDistSq(spec.rect, xMin, yMin);
  float dtr = CalcRoundRectDistSq(spec.rect, xMax, yMin);
  float dbl = CalcRoundRectDistSq(spec.rect, xMin, yMax);
  float dbr = CalcRoundRectDistSq(spec.rect, xMax, yMax);
  float r = spec.rect.r;
  float ri = spec.rect.ri;
  Color interior = spec.rect.interior_color;
  Color outline = spec.rect.outline_color;

  float r_min_sq = (ri - 0.5) * (ri - 0.5);
  // Check if the rect falls entirely inside the interior boundary.
  if (dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq && dbr < r_min_sq) {
    if (spec.fill_mode == FILL_MODE_RECTANGLE || interior.a() > 0) {
      spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_interior);
    }
    return;
  }

  float r_max_sq = (r + 0.5) * (r + 0.5);
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < spec.rect.x0) {
    if (yMax < spec.rect.y0) {
      if (dbr >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    } else if (yMin > spec.rect.y1) {
      if (dtr >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    }
  } else if (xMin > spec.rect.x1) {
    if (yMax < spec.rect.y0) {
      if (dbl >= r_max_sq) {
        if (spec.fill_mode == FILL_MODE_RECTANGLE) {
          spec.out->fillRect(xMin, yMin, xMax, yMax, spec.bgcolor);
        }
        return;
      }
    } else if (yMin > spec.rect.y1) {
      if (dtl >= r_max_sq) {
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
        Color c = GetSmoothRoundRectShapeColor(spec, x, y);
        if (c.a() == 0) continue;
        writer.writePixel(x, y,
                          c == interior  ? spec.pre_blended_interior
                          : c == outline ? spec.pre_blended_outline
                                         : AlphaBlend(spec.bgcolor, c));
      }
    }
  } else {
    Color color[64];
    int cnt = 0;
    for (int16_t y = yMin; y <= yMax; ++y) {
      for (int16_t x = xMin; x <= xMax; ++x) {
        Color c = GetSmoothRoundRectShapeColor(spec, x, y);
        color[cnt++] = c.a() == 0      ? spec.bgcolor
                       : c == interior ? spec.pre_blended_interior
                       : c == outline  ? spec.pre_blended_outline
                                       : AlphaBlend(spec.bgcolor, c);
      }
    }
    spec.out->setAddress(Box(xMin, yMin, xMax, yMax), spec.paint_mode);
    spec.out->write(color, cnt);
  }
}

}  // namespace

void SmoothShape::readColors(const int16_t* x, const int16_t* y, uint32_t count,
                             Color* result) const {
  switch (kind_) {
    case WEDGE: {
      // This default rasterizable implementation seems to be ~50% slower than
      // drawTo (but it allows to use wedges as backgrounds or overlays, e.g.
      // indicator needles).
      uint8_t max_alpha = wedge_.color.a();
      float bax = wedge_.bx - wedge_.ax;
      float bay = wedge_.by - wedge_.ay;
      float bay_dsq = bax * bax + bay * bay;
      WedgeDrawSpec spec{
          // Widen the boundary to simplify calculations.
          .r = wedge_.ar + 0.5f,
          .dr = wedge_.ar - wedge_.br,
          .bax = wedge_.bx - wedge_.ax,
          .bay = wedge_.by - wedge_.ay,
          .hd = bay_dsq,
          .sqrt_hd = sqrtf(bay_dsq),
          .max_alpha = wedge_.color.a(),
          .round_endings = wedge_.round_endings,
      };
      while (count-- > 0) {
        *result++ = wedge_.color.withA(
            GetWedgeShapeAlpha(spec, *x++ - wedge_.ax, *y++ - wedge_.ay));
      }
      break;
    }
    case ROUND_RECT: {
      while (count-- > 0) {
        *result++ = GetSmoothRoundRectColor(round_rect_, *x++, *y++);
      }
      break;
    }
    case EMPTY: {
      while (count-- > 0) {
        *result++ = color::Transparent;
      }
      break;
    }
  }
}

void SmoothShape::drawTo(const Surface& s) const {
  Box box = Box::Intersect(extents_.translate(s.dx(), s.dy()), s.clip_box());
  if (box.empty()) {
    return;
  }
  switch (kind_) {
    case WEDGE: {
      int16_t dx = s.dx();
      int16_t dy = s.dy();
      float ax = wedge_.ax + dx;
      float ay = wedge_.ay + dy;
      float bx = wedge_.bx + dx;
      float by = wedge_.by + dy;
      uint8_t max_alpha = wedge_.color.a();
      float bax = bx - ax;
      float bay = by - ay;
      float bay_dsq = bax * bax + bay * bay;
      WedgeDrawSpec spec{
          .r = wedge_.ar + 0.5f,
          .dr = wedge_.ar - wedge_.br,
          .bax = wedge_.bx - wedge_.ax,
          .bay = wedge_.by - wedge_.ay,
          .hd = bay_dsq,
          .sqrt_hd = sqrtf(bay_dsq),
          .max_alpha = wedge_.color.a(),
          .round_endings = wedge_.round_endings,
      };

      // Establish x start and y start.
      int32_t ys = wedge_.ay;
      if ((ax - wedge_.ar) > (bx - wedge_.br)) ys = wedge_.by;

      uint8_t alpha = max_alpha;
      float xpax, ypay;
      bool can_minimize_scan =
          spec.round_endings && s.fill_mode() != FILL_MODE_RECTANGLE;

      ys += dy;
      if (ys < box.yMin()) ys = box.yMin();
      int32_t xs = box.xMin();
      Color preblended = AlphaBlend(s.bgcolor(), wedge_.color);

      BufferedPixelWriter writer(s.out(), s.paint_mode());

      // Scan bounding box from ys down, calculate pixel intensity from distance
      // to line.
      for (int32_t yp = ys; yp <= box.yMax(); yp++) {
        bool endX = false;  // Flag to skip pixels
        ypay = yp - ay;
        for (int32_t xp = xs; xp <= box.xMax(); xp++) {
          if (endX && alpha == 0 && can_minimize_scan)
            break;  // Skip right side.
          xpax = xp - ax;
          // alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
          alpha = GetWedgeShapeAlpha(spec, xpax, ypay);
          if (alpha == 0 && can_minimize_scan) continue;
          // Track the edge to minimize calculations.
          if (!endX) {
            endX = true;
            if (can_minimize_scan) {
              xs = xp;
            }
          }
          writer.writePixel(
              xp, yp,
              alpha == max_alpha
                  ? preblended
                  : AlphaBlend(s.bgcolor(), wedge_.color.withA(alpha)));
        }
      }

      if (ys > box.yMax()) ys = box.yMax();
      // Reset x start to left side of box.
      xs = box.xMin();
      // Scan bounding box from ys-1 up, calculate pixel intensity from distance
      // to line.
      for (int32_t yp = ys - 1; yp >= box.yMin(); yp--) {
        bool endX = false;  // Flag to skip pixels
        ypay = yp - ay;
        for (int32_t xp = xs; xp <= box.xMax(); xp++) {
          if (endX && alpha == 0 && can_minimize_scan)
            break;  // Skip right side of drawn line.
          xpax = xp - ax;
          // alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
          alpha = GetWedgeShapeAlpha(spec, xpax, ypay);
          if (alpha == 0 && can_minimize_scan) continue;
          // Track line boundary.
          if (!endX) {
            endX = true;
            if (can_minimize_scan) {
              xs = xp;
            }
          }
          writer.writePixel(
              xp, yp,
              alpha == max_alpha
                  ? preblended
                  : AlphaBlend(s.bgcolor(), wedge_.color.withA(alpha)));
        }
      }
      break;
    }
    case ROUND_RECT: {
      RoundRectDrawSpec spec{
          .rect = round_rect_,
          .out = &s.out(),
          .fill_mode = s.fill_mode(),
          .paint_mode = s.paint_mode(),
          .r_squared_adjusted = round_rect_.r * round_rect_.r + 0.25f,
          .ri_squared_adjusted = round_rect_.ri * round_rect_.ri + 0.25f,
          .bgcolor = s.bgcolor(),
          .pre_blended_outline =
              AlphaBlend(AlphaBlend(s.bgcolor(), round_rect_.interior_color),
                         round_rect_.outline_color),
          .pre_blended_interior =
              AlphaBlend(s.bgcolor(), round_rect_.interior_color),
      };
      if (s.dx() != 0 || s.dy() != 0) {
        spec.rect.x0 += s.dx();
        spec.rect.y0 += s.dy();
        spec.rect.x1 += s.dx();
        spec.rect.y1 += s.dy();
        spec.rect.inner_wide = spec.rect.inner_wide.translate(s.dx(), s.dy());
        spec.rect.inner_mid = spec.rect.inner_mid.translate(s.dx(), s.dy());
        spec.rect.inner_tall = spec.rect.inner_tall.translate(s.dx(), s.dy());
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
          FillSmoothRoundRectRectInternal(spec, std::max(x, xMin),
                                          std::max(y, yMin),
                                          std::min((int16_t)(x + 7), xMax),
                                          std::min((int16_t)(y + 7), yMax));
        }
      }
    }
    case EMPTY: {
      return;
    }
  }
}

bool SmoothShape::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                int16_t yMax, Color* result) const {
  switch (kind_) {
    case WEDGE: {
      return Rasterizable::readColorRect(xMin, yMin, xMax, yMax, result);
    }
    case ROUND_RECT: {
      Box box(xMin, yMin, xMax, yMax);
      // Check if the rect happens to fall within the known inner rectangles.
      if (round_rect_.inner_mid.contains(box) ||
          round_rect_.inner_wide.contains(box) ||
          round_rect_.inner_tall.contains(box)) {
        *result = round_rect_.interior_color;
        return true;
      }
      float dtl = CalcRoundRectDistSq(round_rect_, xMin, yMin);
      float dtr = CalcRoundRectDistSq(round_rect_, xMax, yMin);
      float dbl = CalcRoundRectDistSq(round_rect_, xMin, yMax);
      float dbr = CalcRoundRectDistSq(round_rect_, xMax, yMax);
      float r_min_sq = (round_rect_.ri - 0.5) * (round_rect_.ri - 0.5);
      // Check if the rect falls entirely inside the interior boundary.
      if (dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq &&
          dbr < r_min_sq) {
        *result = round_rect_.interior_color;
        return true;
      }

      float r_max_sq = (round_rect_.r + 0.5) * (round_rect_.r + 0.5);
      // Check if the rect falls entirely outside the boundary (in one of the 4
      // corners).
      if (xMax < round_rect_.x0) {
        if (yMax < round_rect_.y0) {
          if (dbr >= r_max_sq) {
            *result = color::Transparent;
            return true;
          }
        } else if (yMax > round_rect_.y1) {
          if (dtr >= r_max_sq) {
            *result = color::Transparent;
            return true;
          }
        }
      } else if (xMax > round_rect_.x1) {
        if (yMax < round_rect_.y0) {
          if (dbl >= r_max_sq) {
            *result = color::Transparent;
            return true;
          }
        } else if (yMax > round_rect_.y1) {
          if (dtl >= r_max_sq) {
            *result = color::Transparent;
            return true;
          }
        }
      }

      Color* out = result;
      for (int16_t y = yMin; y <= yMax; ++y) {
        for (int16_t x = xMin; x <= xMax; ++x) {
          *out++ = GetSmoothRoundRectColorPreChecked(round_rect_, x, y);
        }
      }
      // This is now very unlikely to be true, or we would have caught it above.
      // uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
      // Color c = result[0];
      // for (uint32_t i = 1; i < pixel_count; i++) {
      //   if (result[i] != c) return false;
      // }
      // return true;
      return false;
    }
    case EMPTY: {
      *result = color::Transparent;
      return true;
    }
  }
}

}  // namespace roo_display

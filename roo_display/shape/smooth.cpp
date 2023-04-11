#include "roo_display/shape/smooth.h"

#include <Arduino.h>
#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

SmoothShape::SmoothShape()
    : kind_(SmoothShape::EMPTY), extents_(0, 0, -1, -1) {}

SmoothShape::SmoothShape(Box extents, Wedge wedge)
    : kind_(SmoothShape::WEDGE),
      extents_(std::move(extents)),
      wedge_(std::move(wedge)) {}

SmoothShape::SmoothShape(Box extents, RoundRect round_rect)
    : kind_(SmoothShape::ROUND_RECT),
      extents_(std::move(extents)),
      round_rect_(std::move(round_rect)) {}

SmoothShape::SmoothShape(Box extents, Arc arc)
    : kind_(SmoothShape::ARC),
      extents_(std::move(extents)),
      arc_(std::move(arc)) {}

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
  float ro = radius;
  if (ro > max_radius) ro = max_radius;
  if (ro < 0) ro = 0;
  if (outline_thickness < 0) outline_thickness = 0;
  float ri = ro - outline_thickness;
  if (ri < 0) ri = 0;
  x0 += ro;
  y0 += ro;
  x1 -= ro;
  y1 -= ro;

  float d = sqrtf(0.5f) * ri;
  Box extents((int16_t)roundf(x0 - ro), (int16_t)roundf(y0 - ro),
              (int16_t)roundf(x1 + ro), (int16_t)roundf(y1 + ro));
  Box inner_mid((int16_t)ceilf(x0 - d + 0.5f), (int16_t)ceilf(y0 - d + 0.5f),
                (int16_t)floorf(x1 + d - 0.5f), (int16_t)floorf(y1 + d - 0.5f));
  Box inner_wide((int16_t)ceilf(x0 - ri + 0.5f), (int16_t)ceilf(y0 + 0.5f),
                 (int16_t)floorf(x1 + ri - 0.5f), (int16_t)floorf(y1 - 0.5f));
  Box inner_tall((int16_t)ceilf(x0 + 0.5f), (int16_t)ceilf(y0 - ri + 0.5f),
                 (int16_t)floorf(x1 - 0.5f), (int16_t)floorf(y1 + ri - 0.5f));
  SmoothShape::RoundRect spec{x0,
                              y0,
                              x1,
                              y1,
                              ro,
                              ri,
                              ro * ro + 0.25f,
                              ri * ri + 0.25f,
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

namespace {

inline constexpr int Quadrant(float x, float y) {
  return (x <= 0 && y > 0)   ? 0
         : (y <= 0 && x < 0) ? 1
         : (x >= 0 && y < 0) ? 2
                             : 3;
}

}  // namespace

SmoothShape SmoothArc(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color) {
  return SmoothThickArc(center, radius, 1.0f, angle_start, angle_end, color,
                        ENDING_ROUNDED);
}

SmoothShape SmoothThickArc(FpPoint center, float radius, float thickness,
                           float angle_start, float angle_end, Color color,
                           EndingStyle ending_style) {
  return SmoothThickArcWithBackground(center, radius, thickness, angle_start,
                                      angle_end, color, color::Transparent,
                                      color::Transparent, ending_style);
}

SmoothShape SmoothPie(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color) {
  return SmoothThickArcWithBackground(center, radius, radius, angle_start,
                                      angle_end, color, color::Transparent,
                                      color::Transparent, ENDING_FLAT);
}

SmoothShape SmoothThickArcWithBackground(FpPoint center, float radius,
                                         float thickness, float angle_start,
                                         float angle_end, Color active_color,
                                         Color inactive_color,
                                         Color interior_color,
                                         EndingStyle ending_style) {
  if (radius <= 0 || thickness <= 0 || angle_end == angle_start)
    return SmoothShape();
  if (thickness > radius) thickness = radius;
  if (angle_end < angle_start) {
    std::swap(angle_end, angle_start);
  }
  if (angle_end - angle_start >= 2 * M_PI) {
    return SmoothOutlinedCircle(center, radius, thickness, active_color,
                                interior_color);
  }
  while (angle_start > M_PI) {
    angle_start -= 2 * M_PI;
    angle_end -= 2 * M_PI;
  }

  float start_sin = sinf(angle_start);
  float start_cos = cosf(angle_start);
  float end_sin = sinf(angle_end);
  float end_cos = cosf(angle_end);

  float ro = radius;
  float rm = thickness * 0.5f;
  float rc = radius - thickness * 0.5f;
  float ri = radius - thickness;

  float start_x_ro = ro * start_sin;
  float start_y_ro = -ro * start_cos;
  float start_x_rc = rc * start_sin;
  float start_y_rc = -rc * start_cos;
  float start_x_ri = ri * start_sin;
  float start_y_ri = -ri * start_cos;

  float end_x_ro = ro * end_sin;
  float end_y_ro = -ro * end_cos;
  float end_x_rc = rc * end_sin;
  float end_y_rc = -rc * end_cos;
  float end_x_ri = ri * end_sin;
  float end_y_ri = -ri * end_cos;

  float inv_half_width = 2.0f / thickness;

  int start_quadrant = Quadrant(start_x_ro, start_y_ro);
  int end_quadrant = Quadrant(end_x_ro, end_y_ro);

  float d = sqrtf(0.5f) * (radius - thickness);
  Box inner_mid((int16_t)ceilf(center.x - d + 0.5f),
                (int16_t)ceilf(center.y - d + 0.5f),
                (int16_t)floorf(center.x + d - 0.5f),
                (int16_t)floorf(center.y + d - 0.5f));

  // Figure out the extents.
  bool q1 = (angle_start <= -1.0f * M_PI && angle_end >= -0.5f * M_PI) ||
            (angle_end >= 1.75f * M_PI);
  bool q2 = (angle_start <= -0.5f * M_PI && angle_end >= 0) ||
            (angle_end >= 2.0f * M_PI);
  bool q3 = (angle_start <= 0 && angle_end >= 0.5f * M_PI) ||
            (angle_end >= 2.5f * M_PI);
  bool q4 = (angle_start <= 0.5f * M_PI && angle_end >= M_PI) ||
            (angle_end >= 3.0f * M_PI);
  int16_t xMin, yMin, xMax, yMax;
  if (q1 || q2 || (angle_start <= -0.5f * M_PI && angle_end >= -0.5f * M_PI)) {
    xMin = (int16_t)floorf(center.x - radius - 0.5f);
  } else if (ending_style == ENDING_ROUNDED) {
    xMin = floorf(center.x + std::min(start_x_rc, end_x_rc) - rm - 0.5f);
  } else {
    xMin = floorf(center.x + std::min(std::min(start_x_ro, start_x_ri),
                                      std::min(end_x_ro, end_x_ri) - 0.5f));
  }
  if (q2 || q3 || (angle_start <= 0 && angle_end >= 0)) {
    yMin = (int16_t)floorf(center.y - radius - 0.5f);
  } else if (ending_style == ENDING_ROUNDED) {
    yMin = floorf(center.y + std::min(start_y_rc, end_y_rc) - rm - 0.5f);
  } else {
    yMin = floorf(center.y + std::min(std::min(start_y_ro, start_y_ri),
                                      std::min(end_y_ro, end_y_ri) - 0.5f));
  }
  if (q3 || q4 || (angle_start <= 0.5f * M_PI && angle_end >= 0.5f * M_PI)) {
    xMax = (int16_t)ceilf(center.x + radius + 0.5f);
  } else if (ending_style == ENDING_ROUNDED) {
    xMax = ceilf(center.x + std::max(start_x_rc, end_x_rc) + rm + 0.5f);
  } else {
    xMax = ceilf(center.x + std::max(std::max(start_x_ro, start_x_ri),
                                     std::max(end_x_ro, end_x_ri) + 0.5f));
  }
  if (q4 || q1 || (angle_start <= M_PI && angle_end >= M_PI)) {
    yMax = (int16_t)ceilf(center.y + radius + 0.5f);
  } else if (ending_style == ENDING_ROUNDED) {
    yMax = ceilf(center.y + std::max(start_y_rc, end_y_rc) + rm + 0.5f);
  } else {
    yMax = ceilf(center.y + std::max(std::max(start_y_ro, start_y_ri),
                                     std::max(end_y_ro, end_y_ri) + 0.5f));
  }
  return SmoothShape(
      std::move(Box(xMin, yMin, xMax, yMax)),
      SmoothShape::Arc{center.x,
                       center.y,
                       ro,
                       ri,
                       ro * ro + 0.25f,
                       ri * ri + 0.25f,
                       angle_start,
                       angle_end,
                       active_color,
                       inactive_color,
                       interior_color,
                       inner_mid,
                       (start_x_ro - start_x_rc) * inv_half_width,
                       (start_y_ro - start_y_rc) * inv_half_width,
                       start_x_rc,
                       start_y_rc,
                       (end_x_ro - end_x_rc) * inv_half_width,
                       (end_y_ro - end_y_rc) * inv_half_width,
                       end_x_rc,
                       end_y_rc,
                       start_quadrant,
                       end_quadrant,
                       ending_style == ENDING_ROUNDED});
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

void DrawWedge(const SmoothShape::Wedge& wedge, const Surface& s,
               const Box& box) {
  int16_t dx = s.dx();
  int16_t dy = s.dy();
  float ax = wedge.ax + dx;
  float ay = wedge.ay + dy;
  float bx = wedge.bx + dx;
  float by = wedge.by + dy;
  uint8_t max_alpha = wedge.color.a();
  float bax = bx - ax;
  float bay = by - ay;
  float bay_dsq = bax * bax + bay * bay;
  WedgeDrawSpec spec{
      .r = wedge.ar + 0.5f,
      .dr = wedge.ar - wedge.br,
      .bax = wedge.bx - wedge.ax,
      .bay = wedge.by - wedge.ay,
      .hd = bay_dsq,
      .sqrt_hd = sqrtf(bay_dsq),
      .max_alpha = wedge.color.a(),
      .round_endings = wedge.round_endings,
  };

  // Establish x start and y start.
  int32_t ys = wedge.ay;
  if ((ax - wedge.ar) > (bx - wedge.br)) ys = wedge.by;

  uint8_t alpha = max_alpha;
  float xpax, ypay;
  bool can_minimize_scan =
      spec.round_endings && s.fill_mode() != FILL_MODE_RECTANGLE;

  ys += dy;
  if (ys < box.yMin()) ys = box.yMin();
  int32_t xs = box.xMin();
  Color preblended = AlphaBlend(s.bgcolor(), wedge.color);

  BufferedPixelWriter writer(s.out(), s.paint_mode());

  // Scan bounding box from ys down, calculate pixel intensity from distance
  // to line.
  for (int32_t yp = ys; yp <= box.yMax(); yp++) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0 && can_minimize_scan) break;  // Skip right side.
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
              : AlphaBlend(s.bgcolor(), wedge.color.withA(alpha)));
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
              : AlphaBlend(s.bgcolor(), wedge.color.withA(alpha)));
    }
  }
}

void ReadWedgeColors(const SmoothShape::Wedge& wedge, const int16_t* x,
                     const int16_t* y, uint32_t count, Color* result) {
  // This default rasterizable implementation seems to be ~50% slower than
  // drawTo (but it allows to use wedges as backgrounds or overlays, e.g.
  // indicator needles).
  float bax = wedge.bx - wedge.ax;
  float bay = wedge.by - wedge.ay;
  float bay_dsq = bax * bax + bay * bay;
  WedgeDrawSpec spec{
      // Widen the boundary to simplify calculations.
      .r = wedge.ar + 0.5f,
      .dr = wedge.ar - wedge.br,
      .bax = wedge.bx - wedge.ax,
      .bay = wedge.by - wedge.ay,
      .hd = bay_dsq,
      .sqrt_hd = sqrtf(bay_dsq),
      .max_alpha = wedge.color.a(),
      .round_endings = wedge.round_endings,
  };
  while (count-- > 0) {
    *result++ = wedge.color.withA(
        GetWedgeShapeAlpha(spec, *x++ - wedge.ax, *y++ - wedge.ay));
  }
}

// Helper functions for round rect.

inline Color GetSmoothRoundRectPixelColor(const SmoothShape::RoundRect& rect,
                                          int16_t x, int16_t y) {
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (spec.inner_mid.contains(x, y) || spec.inner_wide.contains(x, y) ||
  //     spec.inner_tall.contains(x, y)) {
  //   return spec.interior;
  // }
  float ref_x = std::min(std::max((float)x, rect.x0), rect.x1);
  float ref_y = std::min(std::max((float)y, rect.y0), rect.y1);
  float dx = x - ref_x;
  float dy = y - ref_y;
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < spec.ri - 0.5) {
  //   return spec.interior;
  // }
  float d_squared = dx * dx + dy * dy;
  float ro = rect.ro;
  float ri = rect.ri;
  Color interior = rect.interior_color;
  Color outline = rect.outline_color;

  if (d_squared <= rect.ri_sq_adj - ri) {
    // Point fully within the interior.
    return interior;
  }
  if (d_squared >= rect.ro_sq_adj + ro) {
    // Point fully outside the round rectangle.
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= rect.ro_sq_adj - ro;
  bool fully_outside_inner = ro == ri || d_squared >= rect.ri_sq_adj + ri;
  if (fully_within_outer && fully_outside_inner) {
    // Point fully within the outline band.
    return outline;
  }
  // Point is on either of the boundaries; need anti-aliasing.
  // Note: replacing with integer sqrt (iterative, loop-unrolled, 24-bit) slows
  // things down. At 64-bit not loop-unrolled, does so dramatically.
  float d = sqrtf(d_squared);
  if (fully_outside_inner) {
    // On the outer boundary.
    return outline.withA((uint8_t)(outline.a() * (ro - d + 0.5f)));
  }
  if (fully_within_outer) {
    // On the inner boundary.
    return AlphaBlend(
        interior,
        outline.withA((uint8_t)(outline.a() * (1.0f - (ri - d + 0.5f)))));
  }
  // On both bounderies (e.g. the band is very thin).
  return AlphaBlend(
      interior, outline.withA((uint8_t)(outline.a() *
                                        std::max(0.0f, (ro - d + 0.5f) -
                                                           (ri - d + 0.5f)))));
}

enum RectColor {
  NON_UNIFORM = 0,
  TRANSPARENT = 1,
  INTERIOR = 2,
  OUTLINE_ACTIVE = 3,
  OUTLINE_INACTIVE = 4,  // Used by arcs.
};

inline float CalcDistSqRect(float x0, float y0, float x1, float y1, int16_t xt,
                            int16_t yt) {
  float dx = (xt <= x0 ? xt - x0 : xt >= x1 ? xt - x1 : 0.0f);
  float dy = (yt <= y0 ? yt - y0 : yt >= y1 ? yt - y1 : 0.0f);
  return dx * dx + dy * dy;
}

// Called for rectangles with area <= 64 pixels.
inline RectColor DetermineRectColorForRoundRect(
    const SmoothShape::RoundRect& rect, const Box& box) {
  if (rect.inner_mid.contains(box) || rect.inner_wide.contains(box) ||
      rect.inner_tall.contains(box)) {
    return INTERIOR;
  }
  int16_t xMin = box.xMin();
  int16_t yMin = box.yMin();
  int16_t xMax = box.xMax();
  int16_t yMax = box.yMax();
  float dtl = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMin, yMin);
  float dtr = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMax, yMin);
  float dbl = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMin, yMax);
  float dbr = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMax, yMax);

  float r_min_sq = rect.ri_sq_adj - rect.ri;
  // Check if the rect falls entirely inside the interior boundary.
  if (dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq && dbr < r_min_sq) {
    return INTERIOR;
  }

  float r_max_sq = rect.ro_sq_adj + rect.ro;
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < rect.x0) {
    if (yMax < rect.y0) {
      if (dbr >= r_max_sq) {
        return TRANSPARENT;
      }
    } else if (yMin > rect.y1) {
      if (dtr >= r_max_sq) {
        return TRANSPARENT;
      }
    }
  } else if (xMin > rect.x1) {
    if (yMax < rect.y0) {
      if (dbl >= r_max_sq) {
        return TRANSPARENT;
      }
    } else if (yMin > rect.y1) {
      if (dtl >= r_max_sq) {
        return TRANSPARENT;
      }
    }
  }
  // If all corners are in the same quadrant, and all corners are within the
  // ring, then the rect is also within the ring.
  if (xMax <= rect.x1) {
    if (yMax <= rect.y1) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtl < r_ring_max_sq && dtl > r_ring_min_sq && dbr < r_ring_max_sq &&
          dbr > r_ring_min_sq) {
        return OUTLINE_ACTIVE;
      }
    } else if (yMin >= rect.y0) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtr < r_ring_max_sq && dtr > r_ring_min_sq && dbl < r_ring_max_sq &&
          dbl > r_ring_min_sq) {
        return OUTLINE_ACTIVE;
      }
    }
  } else if (xMin >= rect.x0) {
    if (yMax <= rect.y1) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtr < r_ring_max_sq && dtr > r_ring_min_sq && dbl < r_ring_max_sq &&
          dbl > r_ring_min_sq) {
        return OUTLINE_ACTIVE;
      }
    } else if (yMin >= rect.y0) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtl < r_ring_max_sq && dtl > r_ring_min_sq && dbr < r_ring_max_sq &&
          dbr > r_ring_min_sq) {
        return OUTLINE_ACTIVE;
      }
    }
  }
  // Slow case; evaluate every pixel from the rectangle.
  return NON_UNIFORM;
}

bool ReadColorRectOfRoundRect(const SmoothShape::RoundRect& rect, int16_t xMin,
                              int16_t yMin, int16_t xMax, int16_t yMax,
                              Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForRoundRect(rect, box)) {
    case TRANSPARENT: {
      *result = color::Transparent;
      return true;
    }
    case INTERIOR: {
      *result = rect.interior_color;
      return true;
    }
    case OUTLINE_ACTIVE: {
      *result = rect.outline_color;
      return true;
    }
    default:
      break;
  }
  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = GetSmoothRoundRectPixelColor(rect, x, y);
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

void ReadRoundRectColors(const SmoothShape::RoundRect& rect, const int16_t* x,
                         const int16_t* y, uint32_t count, Color* result) {
  while (count-- > 0) {
    if (rect.inner_mid.contains(*x, *y) || rect.inner_wide.contains(*x, *y) ||
        rect.inner_tall.contains(*x, *y)) {
      *result = rect.interior_color;
    } else {
      *result = GetSmoothRoundRectPixelColor(rect, *x, *y);
    }
    ++x;
    ++y;
    ++result;
  }
}

struct RoundRectDrawSpec {
  DisplayOutput* out;
  FillMode fill_mode;
  PaintMode paint_mode;
  Color bgcolor;
  Color pre_blended_outline;
  Color pre_blended_interior;
};

// Called for rectangles with area <= 64 pixels.
inline void FillSubrectOfRoundRect(const SmoothShape::RoundRect& rect,
                                   const RoundRectDrawSpec& spec,
                                   const Box& box) {
  Color interior = rect.interior_color;
  Color outline = rect.outline_color;
  switch (DetermineRectColorForRoundRect(rect, box)) {
    case TRANSPARENT: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE) {
        spec.out->fillRect(spec.paint_mode, box, spec.bgcolor);
      }
      return;
    }
    case INTERIOR: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case OUTLINE_ACTIVE: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          outline != color::Transparent) {
        spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_outline);
        return;
      }
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
    BufferedPixelWriter writer(*spec.out, spec.paint_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothRoundRectPixelColor(rect, x, y);
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
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothRoundRectPixelColor(rect, x, y);
        color[cnt++] = c.a() == 0      ? spec.bgcolor
                       : c == interior ? spec.pre_blended_interior
                       : c == outline  ? spec.pre_blended_outline
                                       : AlphaBlend(spec.bgcolor, c);
      }
    }
    spec.out->setAddress(box, spec.paint_mode);
    spec.out->write(color, cnt);
  }
}

void FillSubrectangle(const SmoothShape::RoundRect& rect,
                      const RoundRectDrawSpec& spec, const Box& box) {
  const int16_t xMinOuter = (box.xMin() / 8) * 8;
  const int16_t yMinOuter = (box.yMin() / 8) * 8;
  const int16_t xMaxOuter = (box.xMax() / 8) * 8 + 7;
  const int16_t yMaxOuter = (box.yMax() / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSubrectOfRoundRect(
          rect, spec,
          Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
              std::min((int16_t)(x + 7), box.xMax()),
              std::min((int16_t)(y + 7), box.yMax())));
    }
  }
}

void DrawRoundRect(SmoothShape::RoundRect rect, const Surface& s,
                   const Box& box) {
  RoundRectDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .paint_mode = s.paint_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_outline = AlphaBlend(
          AlphaBlend(s.bgcolor(), rect.interior_color), rect.outline_color),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), rect.interior_color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    rect.x0 += s.dx();
    rect.y0 += s.dy();
    rect.x1 += s.dx();
    rect.y1 += s.dy();
    rect.inner_wide = rect.inner_wide.translate(s.dx(), s.dy());
    rect.inner_mid = rect.inner_mid.translate(s.dx(), s.dy());
    rect.inner_tall = rect.inner_tall.translate(s.dx(), s.dy());
  }
  {
    uint32_t pixel_count = box.area();
    if (pixel_count <= 64) {
      FillSubrectOfRoundRect(rect, spec, box);
      return;
    }
  }
  if (rect.inner_mid.width() <= 16) {
    FillSubrectangle(rect, spec, box);
  } else {
    const Box& inner = rect.inner_mid;
    FillSubrectangle(rect, spec,
                     Box(box.xMin(), box.yMin(), box.xMax(), inner.yMin() - 1));
    FillSubrectangle(
        rect, spec,
        Box(box.xMin(), inner.yMin(), inner.xMin() - 1, inner.yMax()));
    if (s.fill_mode() == FILL_MODE_RECTANGLE ||
        spec.pre_blended_interior != color::Transparent) {
      s.out().fillRect(inner, spec.pre_blended_interior);
    }
    FillSubrectangle(
        rect, spec,
        Box(inner.xMax() + 1, inner.yMin(), box.xMax(), inner.yMax()));
    FillSubrectangle(rect, spec,
                     Box(box.xMin(), inner.yMax() + 1, box.xMax(), box.yMax()));
  }
}

// Arc.

struct ArcDrawSpec {
  DisplayOutput* out;
  FillMode fill_mode;
  PaintMode paint_mode;
  Color bgcolor;
  Color pre_blended_outline_active;
  Color pre_blended_outline_inactive;
  Color pre_blended_interior;
};

Color GetSmoothArcPixelColor(const SmoothShape::Arc& spec, int16_t x,
                             int16_t y) {
  float dx = x - spec.xc;
  float dy = y - spec.yc;
  if (spec.inner_mid.contains(x, y)) {
    return spec.interior_color;
  }
  float d_squared = dx * dx + dy * dy;
  if (d_squared <= spec.ri_sq_adj - spec.ri) {
    // Pixel fully within the 'inner' ring.
    return spec.interior_color;
  }
  if (d_squared >= spec.ro_sq_adj + spec.ro) {
    // Pixel fully outside the 'outer' ring.
    return color::Transparent;
  }
  // We now know that the pixel is somewhere inside the ring.
  Color color = spec.outline_active_color;
  float dxs = dx - spec.start_x_rc;
  float dys = dy - spec.start_y_rc;
  float dxe = dx - spec.end_x_rc;
  float dye = dy - spec.end_y_rc;
  float n1 = spec.start_y_slope * dxs - spec.start_x_slope * dys;
  float n2 = spec.end_x_slope * dye - spec.end_y_slope * dxe;
  bool within_range = false;
  if (spec.angle_end - spec.angle_start > M_PI) {
    within_range = (n1 <= -0.5 || n2 <= -0.5);
  } else {
    within_range = (n1 <= -0.5 && n2 <= -0.5);
  }
  if (!within_range) {
    // Not entirely within the angle range. May be close to boundary; may be far
    // from it. The result depends on ending style.
    if (spec.round_endings) {
      // The endings may overlap, so we need to check the min distance from both
      // endpoints.
      float r = (spec.ro - spec.ri) * 0.5f;
      float r_sq = r * r;
      float smaller_dist_sq =
          std::min(dxs * dxs + dys * dys, dxe * dxe + dye * dye);
      if (smaller_dist_sq > r_sq + r + 0.25f) {
        color = spec.outline_inactive_color;
      } else if (smaller_dist_sq < r_sq - r + 0.25f) {
        color = spec.outline_active_color;
      } else {
        // Round endings boundary - we need to calculate the alpha-blended
        // color.
        float d = sqrt(smaller_dist_sq);
        color = AlphaBlend(
            spec.outline_inactive_color,
            spec.outline_active_color.withA(
                (uint8_t)(spec.outline_active_color.a() * (r - d + 0.5f))));
      }
    } else {
      // Flat endings.
      bool outside_range = false;
      if (spec.angle_end - spec.angle_start > M_PI) {
        outside_range = (n1 >= 0.5f && n2 >= 0.5f);
      } else {
        outside_range = (n1 >= 0.5f || n2 >= 0.5f);
      }
      if (outside_range) {
        color = spec.outline_inactive_color;
      } else {
        float alpha = 1.0;
        if (n1 > -0.5f && n1 < 0.5f) {
          alpha *= (1 - (n1 + 0.5f));
        }
        if (n2 < 0.5f && n2 > -0.5f) {
          alpha *= (1 - (n2 + 0.5f));
        }
        if (alpha > 1.0f) alpha = 1.0f;
        if (alpha < 0.0f) alpha = 0.0f;
        color =
            AlphaBlend(spec.outline_inactive_color,
                       spec.outline_active_color.withA(
                           (uint8_t)(spec.outline_active_color.a() * alpha)));
      }
    }
  }

  // Now we need to apply blending at the edges of the outer and inner rings.
  bool fully_within_outer = d_squared <= spec.ro_sq_adj - spec.ro;
  bool fully_outside_inner = spec.ro == spec.ri ||
                             d_squared >= spec.ri_sq_adj + spec.ri ||
                             spec.ri == 0;

  if (fully_within_outer && fully_outside_inner) {
    // Fully inside the ring, far enough from the boundary.
    return color;
  }
  float d = sqrtf(d_squared);
  if (fully_outside_inner) {
    return color.withA((uint8_t)(color.a() * (spec.ro - d + 0.5f)));
  }
  if (fully_within_outer) {
    return AlphaBlend(
        spec.interior_color,
        color.withA((uint8_t)(color.a() * (1.0f - (spec.ri - d + 0.5f)))));
  }
  return AlphaBlend(
      spec.interior_color,
      color.withA(
          (uint8_t)(color.a() * std::max(0.0f, (spec.ro - d + 0.5f) -
                                                   (spec.ri - d + 0.5f)))));
}

inline float CalcDistSq(float x1, float y1, int16_t x2, int16_t y2) {
  float dx = x1 - x2;
  float dy = y1 - y2;
  return dx * dx + dy * dy;
}

// Called for arcs with area <= 64 pixels.
inline RectColor DetermineRectColorForArc(const SmoothShape::Arc& arc,
                                          const Box& box) {
  if (arc.inner_mid.contains(box)) {
    return INTERIOR;
  }
  int16_t xMin = box.xMin();
  int16_t yMin = box.yMin();
  int16_t xMax = box.xMax();
  int16_t yMax = box.yMax();
  float dtl = CalcDistSq(arc.xc, arc.yc, xMin, yMin);
  float dtr = CalcDistSq(arc.xc, arc.yc, xMax, yMin);
  float dbl = CalcDistSq(arc.xc, arc.yc, xMin, yMax);
  float dbr = CalcDistSq(arc.xc, arc.yc, xMax, yMax);

  float r_min_sq = arc.ri_sq_adj - arc.ri;
  // Check if the rect falls entirely inside the interior boundary.
  if (dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq && dbr < r_min_sq) {
    return INTERIOR;
  }

  float r_max_sq = arc.ro_sq_adj + arc.ro;
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < arc.xc) {
    if (yMax < arc.yc) {
      if (dbr >= r_max_sq) {
        return TRANSPARENT;
      }
    } else if (yMin > arc.yc) {
      if (dtr >= r_max_sq) {
        return TRANSPARENT;
      }
    }
  } else if (xMin > arc.xc) {
    if (yMax < arc.yc) {
      if (dbl >= r_max_sq) {
        return TRANSPARENT;
      }
    } else if (yMin > arc.yc) {
      if (dtl >= r_max_sq) {
        return TRANSPARENT;
      }
    }
  }
  // Slow case; evaluate every pixel from the rectangle.
  return NON_UNIFORM;
}

// Called for arcs with area <= 64 pixels.
void FillSubrectOfArc(const SmoothShape::Arc& arc, const ArcDrawSpec& spec,
                      const Box& box) {
  Color interior = arc.interior_color;
  Color outline_active = arc.outline_active_color;
  Color outline_inactive = arc.outline_inactive_color;
  switch (DetermineRectColorForArc(arc, box)) {
    case TRANSPARENT: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE) {
        spec.out->fillRect(spec.paint_mode, box, spec.bgcolor);
      }
      return;
    }
    case INTERIOR: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.paint_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case OUTLINE_ACTIVE: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          outline_active != color::Transparent) {
        spec.out->fillRect(spec.paint_mode, box,
                           spec.pre_blended_outline_active);
        return;
      }
    }
    case OUTLINE_INACTIVE: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          outline_inactive != color::Transparent) {
        spec.out->fillRect(spec.paint_mode, box,
                           spec.pre_blended_outline_inactive);
        return;
      }
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
    BufferedPixelWriter writer(*spec.out, spec.paint_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothArcPixelColor(arc, x, y);
        if (c.a() == 0) continue;
        writer.writePixel(
            x, y,
            c == interior           ? spec.pre_blended_interior
            : c == outline_active   ? spec.pre_blended_outline_active
            : c == outline_inactive ? spec.pre_blended_outline_inactive
                                    : AlphaBlend(spec.bgcolor, c));
      }
    }
  } else {
    Color color[64];
    int cnt = 0;
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothArcPixelColor(arc, x, y);
        color[cnt++] = c.a() == 0            ? spec.bgcolor
                       : c == interior       ? spec.pre_blended_interior
                       : c == outline_active ? spec.pre_blended_outline_active
                       : c == outline_inactive
                           ? spec.pre_blended_outline_inactive
                           : AlphaBlend(spec.bgcolor, c);
      }
    }
    spec.out->setAddress(box, spec.paint_mode);
    spec.out->write(color, cnt);
  }
}

void DrawArc(SmoothShape::Arc arc, const Surface& s, const Box& box) {
  ArcDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .paint_mode = s.paint_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_outline_active =
          AlphaBlend(AlphaBlend(s.bgcolor(), arc.interior_color),
                     arc.outline_active_color),
      .pre_blended_outline_inactive =
          AlphaBlend(AlphaBlend(s.bgcolor(), arc.interior_color),
                     arc.outline_inactive_color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    arc.xc += s.dx();
    arc.yc += s.dy();
    arc.start_x_rc += s.dx();
    arc.start_y_rc += s.dy();
    arc.end_x_rc += s.dx();
    arc.end_y_rc += s.dy();
    arc.inner_mid = arc.inner_mid.translate(s.dx(), s.dy());
  }
  int16_t xMin = box.xMin();
  int16_t xMax = box.xMax();
  int16_t yMin = box.yMin();
  int16_t yMax = box.yMax();
  {
    uint32_t pixel_count = box.area();
    if (pixel_count <= 64) {
      FillSubrectOfArc(arc, spec, box);
      return;
    }
  }
  const int16_t xMinOuter = (xMin / 8) * 8;
  const int16_t yMinOuter = (yMin / 8) * 8;
  const int16_t xMaxOuter = (xMax / 8) * 8 + 7;
  const int16_t yMaxOuter = (yMax / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSubrectOfArc(arc, spec,
                       Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                           std::min((int16_t)(x + 7), box.xMax()),
                           std::min((int16_t)(y + 7), box.yMax())));
    }
  }
}

bool ReadColorRectOfArc(const SmoothShape::Arc& arc, int16_t xMin, int16_t yMin,
                        int16_t xMax, int16_t yMax, Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForArc(arc, box)) {
    case TRANSPARENT: {
      *result = color::Transparent;
      return true;
    }
    case INTERIOR: {
      *result = arc.interior_color;
      return true;
    }
    case OUTLINE_ACTIVE: {
      *result = arc.outline_active_color;
      return true;
    }
    case OUTLINE_INACTIVE: {
      *result = arc.outline_inactive_color;
      return true;
    }
    default:
      break;
  }
  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = GetSmoothArcPixelColor(arc, x, y);
    }
  }
  // // This is now very unlikely to be true, or we would have caught it above.
  Color c = result[0];
  uint32_t pixel_count = box.area();
  for (uint32_t i = 1; i < pixel_count; i++) {
    if (result[i] != c) return false;
  }
  return true;
}

void ReadArcColors(const SmoothShape::Arc& arc, const int16_t* x,
                   const int16_t* y, uint32_t count, Color* result) {
  while (count-- > 0) {
    *result++ = GetSmoothArcPixelColor(arc, *x++, *y++);
  }
}

}  // namespace

void SmoothShape::drawTo(const Surface& s) const {
  Box box = Box::Intersect(extents_.translate(s.dx(), s.dy()), s.clip_box());
  if (box.empty()) {
    return;
  }
  switch (kind_) {
    case WEDGE: {
      DrawWedge(wedge_, s, box);
      break;
    }
    case ROUND_RECT: {
      DrawRoundRect(round_rect_, s, box);
      break;
    }
    case ARC: {
      DrawArc(arc_, s, box);
      break;
    }
    case EMPTY: {
      return;
    }
  }
}

void SmoothShape::readColors(const int16_t* x, const int16_t* y, uint32_t count,
                             Color* result) const {
  switch (kind_) {
    case WEDGE: {
      ReadWedgeColors(wedge_, x, y, count, result);
      break;
    }
    case ROUND_RECT: {
      ReadRoundRectColors(round_rect_, x, y, count, result);
      break;
    }
    case ARC: {
      ReadArcColors(arc_, x, y, count, result);
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

bool SmoothShape::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                int16_t yMax, Color* result) const {
  switch (kind_) {
    case WEDGE: {
      return Rasterizable::readColorRect(xMin, yMin, xMax, yMax, result);
    }
    case ROUND_RECT: {
      return ReadColorRectOfRoundRect(round_rect_, xMin, yMin, xMax, yMax,
                                      result);
    }
    case ARC: {
      return ReadColorRectOfArc(arc_, xMin, yMin, xMax, yMax, result);
    }
    case EMPTY: {
      *result = color::Transparent;
      return true;
    }
  }
  *result = color::Transparent;
  return true;
}

}  // namespace roo_display

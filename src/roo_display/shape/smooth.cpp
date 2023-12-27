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

SmoothShape::SmoothShape(Box extents, Triangle triangle)
    : kind_(SmoothShape::TRIANGLE),
      extents_(std::move(extents)),
      triangle_(std::move(triangle)) {}

// Used for tiny shapes that fit within one pixel.
SmoothShape::SmoothShape(int16_t x, int16_t y, Pixel pixel)
    : kind_(SmoothShape::PIXEL),
      extents_(x, y, x, y),
      pixel_(std::move(pixel)) {}

SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b, float width_b,
                             Color color, EndingStyle ending_style) {
  if (width_a < 0.0f) width_a = 0.0f;
  if (width_b < 0.0f) width_b = 0.0f;
  if (width_a == 0 && width_b == 0) {
    return SmoothShape();
  }
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
  // if (a.y == b.y && ar == br && ar >= 1.0f) {
  //   // Horizontal line.
  //   if (ending_style == ENDING_ROUNDED) {
  //     return SmoothFilledRoundRect(a.x - ar, a.y - ar, b.x + br, b.y + br,
  //     ar, color);
  //   } else {
  //     // return SmoothFilledRect(a.x, a.y - ar, b.x, b.y + br, color);
  //   }
  // }

  SmoothShape::Wedge wedge{a.x, a.y, b.x,   b.y,
                           ar,  br,  color, (ending_style == ENDING_ROUNDED)};
  ar -= 0.5f;
  br -= 0.5f;
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

SmoothShape SmoothThickRoundRect(float x0, float y0, float x1, float y1,
                                 float radius, float thickness, Color color,
                                 Color interior_color) {
  if (radius < 0) radius = 0;
  if (thickness < 0) thickness = 0;
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);
  x0 -= thickness * 0.5f;
  y0 -= thickness * 0.5f;
  x1 += thickness * 0.5f;
  y1 += thickness * 0.5f;
  radius += thickness * 0.5f;
  float w = x1 - x0;
  float h = y1 - y0;
  float max_radius = ((w < h) ? w : h) / 2;  // 1/2 minor axis.
  if (radius > max_radius) radius = max_radius;
  float ro = radius;
  if (ro < 0) ro = 0;
  float ri = ro - thickness;
  if (ri < 0) ri = 0;
  Box extents((int16_t)floorf(x0 + 0.5f), (int16_t)floorf(y0 + 0.5f),
              (int16_t)ceilf(x1 - 0.5f), (int16_t)ceilf(y1 - 0.5f));
  if (extents.xMin() == extents.xMax() && extents.yMin() == extents.yMax()) {
    // Special case: the rect fits into a single pixel. Let's just calculate the
    // color and get it over with.
    float outer_area = std::min<float>(
        1.0f, std::max<float>(0.0f, w * h - (4.0f - M_PI) * ro * ro));
    float inner_area = std::min<float>(
        1.0f, std::max<float>(0.0f, (w - thickness) * (h - thickness) -
                                        (4.0f - M_PI) * ri * ri));
    color = color.withA(color.a() * outer_area);
    interior_color = interior_color.withA(interior_color.a() * inner_area);
    Color pixel_color = AlphaBlend(color, interior_color);
    if (pixel_color.a() == 0) return SmoothShape();
    return SmoothShape(extents.xMin(), extents.yMin(),
                       SmoothShape::Pixel{color});
  }
  x0 += ro;
  y0 += ro;
  x1 -= ro;
  y1 -= ro;
  float d = sqrtf(0.5f) * ri;
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
                              color,
                              interior_color,
                              std::move(inner_mid),
                              std::move(inner_wide),
                              std::move(inner_tall)};

  return SmoothShape(std::move(extents), std::move(spec));
}

SmoothShape SmoothRoundRect(float x0, float y0, float x1, float y1,
                            float radius, Color color, Color interior_color) {
  return SmoothThickRoundRect(x0, y0, x1, y1, radius, 1, color, interior_color);
}

SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                                  float radius, Color color) {
  return SmoothThickRoundRect(x0, y0, x1, y1, radius, 0, color, color);
}

// SmoothShape SmoothFilledRect(float x0, float y0, float x1, float y1,
//                              Color color) {
//   return SmoothFilledRoundRect(x0, y0, x1, y1, 0.5, color);
// }

SmoothShape SmoothThickCircle(FpPoint center, float radius, float thickness,
                              Color color, Color interior_color) {
  return SmoothThickRoundRect(center.x - radius, center.y - radius,
                              center.x + radius, center.y + radius, radius,
                              thickness, color, interior_color);
}

SmoothShape SmoothCircle(FpPoint center, float radius, Color color,
                         Color interior_color) {
  return SmoothThickCircle(center, radius, 1, color, interior_color);
}

SmoothShape SmoothFilledCircle(FpPoint center, float radius, Color color) {
  return SmoothThickCircle(center, radius, 0, color, color);
}

namespace {

inline constexpr int Quadrant(float x, float y) {
  // 1 | 0
  // --+--
  // 2 | 3
  return (x <= 0 && y > 0)   ? 0
         : (y <= 0 && x < 0) ? 1
         : (x >= 0 && y < 0) ? 2
                             : 3;
}

}  // namespace

SmoothShape SmoothThickArcImpl(FpPoint center, float radius, float thickness,
                               float angle_start, float angle_end,
                               Color active_color, Color inactive_color,
                               Color interior_color, EndingStyle ending_style,
                               bool trim_to_active) {
  if (radius <= 0 || thickness <= 0 || angle_end == angle_start) {
    return SmoothShape();
  }
  if (angle_end < angle_start) {
    std::swap(angle_end, angle_start);
  }
  if (angle_end - angle_start >= 2 * M_PI) {
    return SmoothThickCircle(center, radius, thickness, active_color,
                             interior_color);
  }
  radius += thickness * 0.5f;
  while (angle_start >= M_PI) {
    angle_start -= 2 * M_PI;
    angle_end -= 2 * M_PI;
  }

  float start_sin = sinf(angle_start);
  float start_cos = cosf(angle_start);
  float end_sin = sinf(angle_end);
  float end_cos = cosf(angle_end);

  float ro = radius;
  float ri = std::max(0.0f, ro - thickness);
  thickness = ro - ri;
  float rc = (ro + ri) * 0.5f;
  float rm = (ro - ri) * 0.5f;

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

  // int start_quadrant = Quadrant(start_x_ro, start_y_ro);
  // int end_quadrant = Quadrant(end_x_ro, end_y_ro);

  float d = sqrtf(0.5f) * (radius - thickness);
  Box inner_mid((int16_t)ceilf(center.x - d + 0.5f),
                (int16_t)ceilf(center.y - d + 0.5f),
                (int16_t)floorf(center.x + d - 0.5f),
                (int16_t)floorf(center.y + d - 0.5f));

  bool has_nonempty_cutoff;
  float cutoff_angle;
  float cutoff_start_sin;
  float cutoff_start_cos;
  float cutoff_end_sin;
  float cutoff_end_cos;
  if (ending_style == ENDING_ROUNDED) {
    cutoff_angle = 2.0f * asinf(rm / (2.0f * (ro - rm)));
    has_nonempty_cutoff =
        (angle_end - angle_start + 2.0f * cutoff_angle) < 2.0f * M_PI;
    cutoff_start_sin = sinf(angle_start - cutoff_angle);
    cutoff_start_cos = cosf(angle_start - cutoff_angle);
    cutoff_end_sin = sinf(angle_end + cutoff_angle);
    cutoff_end_cos = cosf(angle_end + cutoff_angle);
  } else {
    has_nonempty_cutoff = true;
    cutoff_angle = 0.0f;
    cutoff_start_sin = start_sin;
    cutoff_start_cos = start_cos;
    cutoff_end_sin = end_sin;
    cutoff_end_cos = end_cos;
  }

  // qt0-qt3, when true, mean that the arc goes through the entire given
  // quadrant, in which case the bounds are determined simply by the external
  // radius. qf0-qf3 are defined similarly, except they indicate that the arc
  // goes through none of the quadrant.
  //
  // Quadrants are defined as follows:
  //
  // 0 | 1
  // --+--
  // 2 | 3

  bool qt0 = (angle_start <= -0.5f * M_PI && angle_end >= 0) ||
             (angle_end >= 2.0f * M_PI);
  bool qt1 = (angle_start <= 0 && angle_end >= 0.5f * M_PI) ||
             (angle_end >= 2.5f * M_PI);
  bool qt2 = (angle_start <= -1.0f * M_PI && angle_end >= -0.5f * M_PI) ||
             (angle_end >= 1.5 * M_PI);
  bool qt3 = (angle_start <= 0.5f * M_PI && angle_end >= M_PI) ||
             (angle_end >= 3.0f * M_PI);

  bool qf0, qf1, qf2, qf3;
  if (!has_nonempty_cutoff || trim_to_active) {
    qf0 = qf1 = qf2 = qf3 = false;
  } else {
    float cutoff_angle_start = angle_start - cutoff_angle;
    float cutoff_angle_end = angle_end + cutoff_angle;
    qf0 = !qt0 && ((cutoff_angle_end < -0.5f * M_PI) ||
                   (cutoff_angle_start > 0 && cutoff_angle_end < 1.5f * M_PI));
    qf1 =
        !qt1 &&
        ((cutoff_angle_end < 0.0f * M_PI) ||
         (cutoff_angle_start > 0.5f * M_PI && cutoff_angle_end < 2.0f * M_PI));
    qf2 = !qt2 &&
          (cutoff_angle_start > -0.5f * M_PI && cutoff_angle_end < 1.0f * M_PI);
    qf3 =
        !qt3 &&
        (cutoff_angle_end < 0.5f * M_PI ||
         (cutoff_angle_start > 1.0f * M_PI && cutoff_angle_end < 2.5f * M_PI));
  }

  // Figure out the extents.
  int16_t xMin, yMin, xMax, yMax;
  if (trim_to_active) {
    if (qt0 || qt1 || (angle_start <= 0 && angle_end >= 0)) {
      // Arc passes through zero (touches the top).
      yMin = (int16_t)floorf(center.y - radius);
    } else if (ending_style == ENDING_ROUNDED) {
      yMin = floorf(center.y + std::min(start_y_rc, end_y_rc) - rm);
    } else {
      yMin = floorf(center.y + std::min(std::min(start_y_ro, start_y_ri),
                                        std::min(end_y_ro, end_y_ri)));
    }
    if (qt0 || qt2 ||
        (angle_start <= -0.5f * M_PI && angle_end >= -0.5f * M_PI)) {
      // Arc passes through -M_PI/2 (touches the left).
      xMin = (int16_t)floorf(center.x - radius);
    } else if (ending_style == ENDING_ROUNDED) {
      xMin = floorf(center.x + std::min(start_x_rc, end_x_rc) - rm);
    } else {
      xMin = floorf(center.x + std::min(std::min(start_x_ro, start_x_ri),
                                        std::min(end_x_ro, end_x_ri)));
    }
    if (qt2 || qt3 || (angle_start <= M_PI && angle_end >= M_PI)) {
      // Arc passes through M_PI (touches the bottom).
      yMax = (int16_t)ceilf(center.y + radius);
    } else if (ending_style == ENDING_ROUNDED) {
      yMax = ceilf(center.y + std::max(start_y_rc, end_y_rc) + rm);
    } else {
      yMax = ceilf(center.y + std::max(std::max(start_y_ro, start_y_ri),
                                       std::max(end_y_ro, end_y_ri)));
    }
    if (qt3 || qt1 ||
        (angle_start <= 0.5f * M_PI && angle_end >= 0.5f * M_PI)) {
      // Arc passes through M_PI/2 (touches the right).
      xMax = (int16_t)ceilf(center.x + radius);
    } else if (ending_style == ENDING_ROUNDED) {
      xMax = ceilf(center.x + std::max(start_x_rc, end_x_rc) + rm);
    } else {
      xMax = ceilf(center.x + std::max(std::max(start_x_ro, start_x_ri),
                                       std::max(end_x_ro, end_x_ri)));
    }
  } else {
    xMin = (int16_t)floorf(center.x - radius);
    yMin = (int16_t)floorf(center.y - radius);
    xMax = (int16_t)ceilf(center.x + radius);
    yMax = (int16_t)ceilf(center.y + radius);
  }

  return SmoothShape(
      Box(xMin, yMin, xMax, yMax),
      SmoothShape::Arc{center.x,
                       center.y,
                       ro,
                       ri,
                       rm,
                       ro * ro + 0.25f,
                       ri * ri + 0.25f,
                       rm * rm + 0.25f,
                       angle_start,
                       angle_end,
                       active_color,
                       inactive_color,
                       interior_color,
                       inner_mid,
                       start_sin,
                       -start_cos,
                       start_x_rc,
                       start_y_rc,
                       end_sin,
                       -end_cos,
                       end_x_rc,
                       end_y_rc,
                       cutoff_start_sin,
                       -cutoff_start_cos,
                       cutoff_end_sin,
                       -cutoff_end_cos,
                       ending_style == ENDING_ROUNDED,
                       angle_end - angle_start <= M_PI,
                       has_nonempty_cutoff,
                       angle_end - angle_start + 2.0f * cutoff_angle < M_PI,
                       (uint8_t)(((uint8_t)qt0 << 0) | ((uint8_t)qt1 << 1) |
                                 ((uint8_t)qt2 << 2) | ((uint8_t)qt3 << 3) |
                                 ((uint8_t)qf0 << 4) | ((uint8_t)qf1 << 5) |
                                 ((uint8_t)qf2 << 6) | ((uint8_t)qf3 << 7))});
}

SmoothShape SmoothArc(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color) {
  return SmoothThickArc(center, radius, 1.0f, angle_start, angle_end, color,
                        ENDING_ROUNDED);
}

SmoothShape SmoothThickArc(FpPoint center, float radius, float thickness,
                           float angle_start, float angle_end, Color color,
                           EndingStyle ending_style) {
  return SmoothThickArcImpl(center, radius, thickness, angle_start, angle_end,
                            color, color::Transparent, color::Transparent,
                            ending_style, true);
}

SmoothShape SmoothPie(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color) {
  return SmoothThickArcImpl(center, radius * 0.5f, radius, angle_start,
                            angle_end, color, color::Transparent,
                            color::Transparent, ENDING_FLAT, true);
}

SmoothShape SmoothThickArcWithBackground(FpPoint center, float radius,
                                         float thickness, float angle_start,
                                         float angle_end, Color active_color,
                                         Color inactive_color,
                                         Color interior_color,
                                         EndingStyle ending_style) {
  return SmoothThickArcImpl(center, radius, thickness, angle_start, angle_end,
                            active_color, inactive_color, interior_color,
                            ending_style, false);
}

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

  BufferedPixelWriter writer(s.out(), s.blending_mode());

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
      if (alpha != 0 || s.fill_mode() == FILL_MODE_RECTANGLE) {
        writer.writePixel(
            xp, yp,
            alpha == max_alpha
                ? preblended
                : AlphaBlend(s.bgcolor(), wedge.color.withA(alpha)));
      }
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
      if (alpha != 0 || s.fill_mode() == FILL_MODE_RECTANGLE) {
        writer.writePixel(
            xp, yp,
            alpha == max_alpha
                ? preblended
                : AlphaBlend(s.bgcolor(), wedge.color.withA(alpha)));
      }
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
  // if (rect.inner_mid.contains(x, y) || rect.inner_wide.contains(x, y) ||
  //     rect.inner_tall.contains(x, y)) {
  //   return rect.interior_color;
  // }
  float ref_x = std::min(std::max((float)x, rect.x0), rect.x1);
  float ref_y = std::min(std::max((float)y, rect.y0), rect.y1);
  float dx = x - ref_x;
  float dy = y - ref_y;

  Color interior = rect.interior_color;
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < spec.ri - 0.5) {
  //   return spec.interior;
  // }
  float d_squared = dx * dx + dy * dy;
  float ro = rect.ro;
  float ri = rect.ri;
  Color outline = rect.outline_color;

  if (d_squared <= rect.ri_sq_adj - ri && ri >= 0.5f) {
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
  // if (dx == 0 && dy == 0 && rect.inner_mid.contains(x, y)) {
  //   return interior;
  // }
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
    float opacity = 1.0f - (ri - d + 0.5f);
    if (ri < 0.5f && (roundf(rect.x0 - ri) == roundf(rect.x1 + ri) ||
                      roundf(rect.y0 - ri) == roundf(rect.y1 + ri))) {
      // Pesky corner case: the interior is hair-thin. Approximate.
      opacity = std::min(1.0f, opacity * 2.0f);
    }
    return AlphaBlend(interior,
                      outline.withA((uint8_t)(outline.a() * opacity)));
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

inline float CalcDistSqRect(float x0, float y0, float x1, float y1, float xt,
                            float yt) {
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
  float xMin = box.xMin() - 0.5f;
  float yMin = box.yMin() - 0.5f;
  float xMax = box.xMax() + 0.5f;
  float yMax = box.yMax() + 0.5f;
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
  BlendingMode blending_mode;
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
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case INTERIOR: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case OUTLINE_ACTIVE: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          outline != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_outline);
        return;
      }
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
    BufferedPixelWriter writer(*spec.out, spec.blending_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothRoundRectPixelColor(rect, x, y);
        if (c == color::Transparent) continue;
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
    spec.out->setAddress(box, spec.blending_mode);
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
      .blending_mode = s.blending_mode(),
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
        rect.interior_color != color::Transparent) {
      s.out().fillRect(s.blending_mode(), inner, spec.pre_blended_interior);
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
  BlendingMode blending_mode;
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
  if (spec.ri >= 0.5f && d_squared <= spec.ri_sq_adj - spec.ri) {
    // Pixel fully within the 'inner' ring.
    return spec.interior_color;
  }
  if (d_squared >= spec.ro_sq_adj + spec.ro) {
    // Pixel fully outside the 'outer' ring.
    return color::Transparent;
  }
  // We now know that the pixel is somewhere inside the ring.
  Color color;

  // 0 | 1
  // --+--
  // 2 | 3

  int qx = (dx < 0.5) << 0 | (dx > -0.5) << 1;
  int quadrant = (((dy < 0.5) * 3) & qx) | (((dy > -0.5)) * 3 & qx) << 2;

  if ((quadrant & spec.quadrants_) == quadrant) {
    // The entire quadrant is within range.
    color = spec.outline_active_color;
  } else if ((quadrant & (spec.quadrants_ >> 4)) == quadrant) {
    // The entire quadrant is outside range.
    color = spec.outline_inactive_color;
  } else {
    bool within_range = false;
    float n1 = spec.start_y_slope * dx - spec.start_x_slope * dy;
    float n2 = spec.end_x_slope * dy - spec.end_y_slope * dx;
    if (spec.range_angle_sharp) {
      within_range = (n1 <= -0.5 && n2 <= -0.5);
    } else {
      within_range = (n1 <= -0.5 || n2 <= -0.5);
    }
    if (within_range) {
      color = spec.outline_active_color;
    } else {
      // Not entirely within the angle range. May be close to boundary; may be
      // far from it. The result depends on ending style.
      if (spec.round_endings) {
        float dxs = dx - spec.start_x_rc;
        float dys = dy - spec.start_y_rc;
        float dxe = dx - spec.end_x_rc;
        float dye = dy - spec.end_y_rc;
        // The endings may overlap, so we need to check the min distance from
        // both endpoints.
        float smaller_dist_sq =
            std::min(dxs * dxs + dys * dys, dxe * dxe + dye * dye);
        if (smaller_dist_sq > spec.rm_sq_adj + spec.rm) {
          color = spec.outline_inactive_color;
        } else if (smaller_dist_sq < spec.rm_sq_adj - spec.rm) {
          color = spec.outline_active_color;
        } else {
          // Round endings boundary - we need to calculate the alpha-blended
          // color.
          float d = sqrt(smaller_dist_sq);
          color = AlphaBlend(spec.outline_inactive_color,
                             spec.outline_active_color.withA((
                                 uint8_t)(0.5f + spec.outline_active_color.a() *
                                                     (spec.rm - d + 0.5f))));
        }
      } else {
        if (spec.range_angle_sharp) {
          bool outside_range = (n1 >= 0.5f || n2 >= 0.5f);
          if (outside_range) {
            color = spec.outline_inactive_color;
          } else {
            float alpha = 1.0f;
            if (n1 > -0.5f && n1 < 0.5f &&
                spec.start_x_slope * dx + spec.start_y_slope * dy > 0) {
              // To the inner side of the start angle sub-plane (shifted by
              // 0.5), and pointing towards the start angle (cos < 90).
              alpha *= (1 - (n1 + 0.5f));
            }
            if (n2 < 0.5f && n2 > -0.5f &&
                spec.end_x_slope * dx + spec.end_y_slope * dy >= 0) {
              // To the inner side of the end angle sub-plane (shifted by 0.5),
              // and pointing towards the end angle (cos < 90).
              alpha *= (1 - (n2 + 0.5f));
            }
            color = AlphaBlend(
                spec.outline_inactive_color,
                spec.outline_active_color.withA(
                    (uint8_t)(0.5f + spec.outline_active_color.a() * alpha)));
          }
        } else {
          // Equivalent to the 'sharp' case with colors (active vs active)
          // flipped.
          bool inside_range = (n1 <= -0.5f || n2 <= -0.5f);
          if (inside_range) {
            color = spec.outline_active_color;
          } else {
            float alpha = 1.0f;
            if (n1 > -0.5f && n1 < 0.5f &&
                spec.start_y_slope * dy + spec.start_x_slope * dx >= 0) {
              // To the inner side of the start angle sub-plane (shifted by
              // 0.5), and pointing towards the start angle (cos < 90).
              alpha *= (n1 + 0.5f);
            }
            if (n2 < 0.5f && n2 > -0.5f &&
                spec.end_x_slope * dx + spec.end_y_slope * dy > 0) {
              // To the inner side of the end angle sub-plane (shifted by 0.5),
              // and pointing towards the end angle (cos < 90).
              alpha *= (n2 + 0.5f);
            }
            alpha = 1.0f - alpha;
            color = AlphaBlend(
                spec.outline_inactive_color,
                spec.outline_active_color.withA(
                    (uint8_t)(0.5f + spec.outline_active_color.a() * alpha)));
          }
        }
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

inline bool IsRectWithinAngle(float start_x_slope, float start_y_slope,
                              float end_x_slope, float end_y_slope, bool sharp,
                              float cx, float cy, const Box& box) {
  float dxl = box.xMin() - cx;
  float dxr = box.xMax() - cx;
  float dyt = box.yMin() - cy;
  float dyb = box.yMax() - cy;
  if (sharp) {
    return (start_y_slope * dxl - start_x_slope * dyt <= -0.5f &&
            start_y_slope * dxl - start_x_slope * dyb <= -0.5f &&
            start_y_slope * dxr - start_x_slope * dyt <= -0.5f &&
            start_y_slope * dxr - start_x_slope * dyb <= -0.5f) &&
           (end_x_slope * dyt - end_y_slope * dxl <= -0.5f &&
            end_x_slope * dyb - end_y_slope * dxl <= -0.5f &&
            end_x_slope * dyt - end_y_slope * dxr <= -0.5f &&
            end_x_slope * dyb - end_y_slope * dxr <= -0.5f);
  } else {
    return (start_y_slope * dxl - start_x_slope * dyt <= -0.5f &&
            start_y_slope * dxl - start_x_slope * dyb <= -0.5f &&
            start_y_slope * dxr - start_x_slope * dyt <= -0.5f &&
            start_y_slope * dxr - start_x_slope * dyb <= -0.5f) ||
           (end_x_slope * dyt - end_y_slope * dxl <= -0.5f &&
            end_x_slope * dyb - end_y_slope * dxl <= -0.5f &&
            end_x_slope * dyt - end_y_slope * dxr <= -0.5f &&
            end_x_slope * dyb - end_y_slope * dxr <= -0.5f);
  }
}

inline bool IsPointWithinCircle(float cx, float cy, float r_sq, float x,
                                float y) {
  float dx = x - cx;
  float dy = y - cy;
  return dx * dx + dy * dy <= r_sq;
}

inline bool IsRectWithinCircle(float cx, float cy, float r_sq, const Box& box) {
  return IsPointWithinCircle(cx, cy, r_sq, box.xMin(), box.yMin()) &&
         IsPointWithinCircle(cx, cy, r_sq, box.xMin(), box.yMax()) &&
         IsPointWithinCircle(cx, cy, r_sq, box.xMax(), box.yMin()) &&
         IsPointWithinCircle(cx, cy, r_sq, box.xMax(), box.yMax());
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
  if (arc.round_endings) {
    if (arc.nonempty_cutoff) {
      // Check if all rect corners are in the cut-out area.
    }
  }
  // If all corners are in the same quadrant, and all corners are within the
  // ring, then the rect is also within the ring.
  if (xMax <= arc.xc) {
    if (yMax <= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dbr > r_ring_max_sq ||
          dbr < r_ring_min_sq) {
        return NON_UNIFORM;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if ((arc.quadrants_ & 1) && xMax <= arc.xc - 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return OUTLINE_ACTIVE;
      }
      if ((arc.quadrants_ & 0x10) && xMax <= arc.xc - 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return OUTLINE_INACTIVE;
      }
    } else if (yMin >= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtr > r_ring_max_sq || dtr < r_ring_min_sq || dbl > r_ring_max_sq ||
          dbl < r_ring_min_sq) {
        return NON_UNIFORM;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if (arc.quadrants_ & 4 && xMax <= arc.xc - 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return OUTLINE_ACTIVE;
      }
      if (arc.quadrants_ & 0x40 && xMax <= arc.xc - 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return OUTLINE_INACTIVE;
      }
    } else if (xMax <= arc.xc - arc.ri - 0.5f) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dbl > r_ring_max_sq ||
          dbl < r_ring_min_sq) {
        return NON_UNIFORM;
        // Fast-path for the case when the arc contains the entire half-circle.
        if ((arc.quadrants_ & 0b0101) == 0b0101) {
          return OUTLINE_ACTIVE;
        }
      }
    } else {
      return NON_UNIFORM;
    }
  } else if (xMin >= arc.xc) {
    if (yMax <= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtr > r_ring_max_sq || dtr < r_ring_min_sq || dbl > r_ring_max_sq ||
          dbl < r_ring_min_sq) {
        return NON_UNIFORM;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if (arc.quadrants_ & 2 && xMin >= arc.xc + 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return OUTLINE_ACTIVE;
      }
      if (arc.quadrants_ & 0x20 && xMin >= arc.xc + 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return OUTLINE_INACTIVE;
      }
    } else if (yMin >= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dbr > r_ring_max_sq ||
          dbr < r_ring_min_sq) {
        return NON_UNIFORM;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if (arc.quadrants_ & 8 && xMin >= arc.xc + 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return OUTLINE_ACTIVE;
      }
      if (arc.quadrants_ & 0x80 && xMin >= arc.xc + 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return OUTLINE_INACTIVE;
      }
    } else if (xMin >= arc.xc + arc.ri + 0.5f) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtr > r_ring_max_sq || dtr < r_ring_min_sq || dbr > r_ring_max_sq ||
          dbr < r_ring_min_sq) {
        return NON_UNIFORM;
      }
      // Fast-path for the case when the arc contains the entire half-circle.
      if ((arc.quadrants_ & 0b1010) == 0b1010) {
        return OUTLINE_ACTIVE;
      }
    } else {
      return NON_UNIFORM;
    }
  } else if (yMax <= arc.yc - arc.ri - 0.5f) {
    float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
    float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
    if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dtr > r_ring_max_sq ||
        dtr < r_ring_min_sq) {
      return NON_UNIFORM;
    }
    // Fast-path for the case when the arc contains the entire half-circle.
    if ((arc.quadrants_ & 0b0011) == 0b0011) {
      return OUTLINE_ACTIVE;
    }
  } else if (yMin >= arc.yc + arc.ri + 0.5f) {
    float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
    float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
    if (dbl > r_ring_max_sq || dbl < r_ring_min_sq || dbr > r_ring_max_sq ||
        dbr < r_ring_min_sq) {
      return NON_UNIFORM;
    }
    // // Fast-path for the case when the arc contains the entire half-circle.
    if ((arc.quadrants_ & 0b1100) == 0b1100) {
      return OUTLINE_ACTIVE;
    }
  } else {
    return NON_UNIFORM;
  }
  // The rectangle is entirely inside the ring. Let's check if it is actually
  // single-color.
  //
  // First, let's see if the rect is entirely within the 'active' angle.
  if (IsRectWithinAngle(arc.start_x_slope, arc.start_y_slope, arc.end_x_slope,
                        arc.end_y_slope, arc.range_angle_sharp, arc.xc, arc.yc,
                        box)) {
    return OUTLINE_ACTIVE;
  }

  // Now, let's see if the rect is perhaps entirely inside the 'inactive' angle.
  if (arc.nonempty_cutoff &&
      IsRectWithinAngle(arc.end_cutoff_x_slope, arc.end_cutoff_y_slope,
                        arc.start_cutoff_x_slope, arc.start_cutoff_y_slope,
                        !arc.cutoff_angle_sharp, arc.xc, arc.yc, box)) {
    return OUTLINE_INACTIVE;
  }

  // Finally, check if maybe the rect is entirely within one of the round
  // endings.
  if (arc.round_endings) {
    if (IsRectWithinCircle(arc.xc + arc.start_x_rc, arc.yc + arc.start_y_rc,
                           arc.rm_sq_adj - arc.rm, box) ||
        IsRectWithinCircle(arc.xc + arc.end_x_rc, arc.yc + arc.end_y_rc,
                           arc.rm_sq_adj - arc.rm, box)) {
      return OUTLINE_ACTIVE;
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
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case INTERIOR: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case OUTLINE_ACTIVE: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          outline_active != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box,
                           spec.pre_blended_outline_active);
        return;
      }
    }
    case OUTLINE_INACTIVE: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          outline_inactive != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box,
                           spec.pre_blended_outline_inactive);
        return;
      }
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
    BufferedPixelWriter writer(*spec.out, spec.blending_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothArcPixelColor(arc, x, y);
        if (c == color::Transparent) continue;
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
    spec.out->setAddress(box, spec.blending_mode);
    spec.out->write(color, cnt);
  }
}

void DrawArc(SmoothShape::Arc arc, const Surface& s, const Box& box) {
  ArcDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .blending_mode = s.blending_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_outline_active =
          AlphaBlend(AlphaBlend(s.bgcolor(), arc.interior_color),
                     arc.outline_active_color),
      .pre_blended_outline_inactive =
          AlphaBlend(AlphaBlend(s.bgcolor(), arc.interior_color),
                     arc.outline_inactive_color),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), arc.interior_color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    arc.xc += s.dx();
    arc.yc += s.dy();
    // Not adjusting {start,end}_{x,y}_rc, because those are relative to (xc,
    // yc).
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

// Triangle.

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
inline RectColor DetermineRectColorForTriangle(
    const SmoothShape::Triangle& triangle, const Box& box) {
  // First, let's see if the rect is entirely within the triangle.
  if (IsRectWithinTriangle(triangle, box)) {
    return INTERIOR;
  }
  if (IsRectOutsideTriangle(triangle, box)) {
    return TRANSPARENT;
  }
  return NON_UNIFORM;
}

// Called for arcs with area <= 64 pixels.
void FillSubrectOfTriangle(const SmoothShape::Triangle& triangle,
                           const TriangleDrawSpec& spec, const Box& box) {
  Color interior = triangle.color;
  switch (DetermineRectColorForTriangle(triangle, box)) {
    case TRANSPARENT: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE) {
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case INTERIOR: {
      if (spec.fill_mode == FILL_MODE_RECTANGLE ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FILL_MODE_VISIBLE) {
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

void DrawTriangle(SmoothShape::Triangle triangle, const Surface& s,
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

bool ReadColorRectOfTriangle(const SmoothShape::Triangle& triangle,
                             int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForTriangle(triangle, box)) {
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

void DrawPixel(SmoothShape::Pixel pixel, const Surface& s, const Box& box) {
  int16_t x = box.xMin();
  int16_t y = box.yMin();
  s.out().fillPixels(s.blending_mode(), AlphaBlend(s.bgcolor(), pixel.color),
                     &x, &y, 1);
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
    case TRIANGLE: {
      DrawTriangle(triangle_, s, box);
      break;
    }
    case PIXEL: {
      DrawPixel(pixel_, s, box);
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
    case TRIANGLE: {
      ReadTriangleColors(triangle_, x, y, count, result);
      break;
    }
    case PIXEL: {
      while (count-- > 0) {
        *result = pixel_.color;
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
    case TRIANGLE: {
      return ReadColorRectOfTriangle(triangle_, xMin, yMin, xMax, yMax, result);
    }
    case PIXEL: {
      *result = pixel_.color;
      return true;
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

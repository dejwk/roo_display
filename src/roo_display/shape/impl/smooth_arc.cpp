#include "roo_display/shape/impl/smooth_arc.h"

#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

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
    cutoff_angle = asinf(rm / rc);
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
    if (cutoff_angle_start < -M_PI) {
      cutoff_angle_start += 2 * M_PI;
      cutoff_angle_end += 2 * M_PI;
    }
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

namespace {

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
internal::arc::AreaType DetermineRectColorForArcImpl(
    const SmoothShape::Arc& arc, const Box& box) {
  if (arc.inner_mid.contains(box)) {
    return internal::arc::AreaType::kInterior;
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
    return internal::arc::AreaType::kInterior;
  }

  float r_max_sq = arc.ro_sq_adj + arc.ro;
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < arc.xc) {
    if (yMax < arc.yc) {
      if (dbr >= r_max_sq) {
        return internal::arc::AreaType::kExterior;
      }
    } else if (yMin > arc.yc) {
      if (dtr >= r_max_sq) {
        return internal::arc::AreaType::kExterior;
      }
    }
  } else if (xMin > arc.xc) {
    if (yMax < arc.yc) {
      if (dbl >= r_max_sq) {
        return internal::arc::AreaType::kExterior;
      }
    } else if (yMin > arc.yc) {
      if (dtl >= r_max_sq) {
        return internal::arc::AreaType::kExterior;
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
        return internal::arc::AreaType::kMixed;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if ((arc.quadrants_ & 1) && xMax <= arc.xc - 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return internal::arc::AreaType::kOutlineActive;
      }
      if ((arc.quadrants_ & 0x10) && xMax <= arc.xc - 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return internal::arc::AreaType::kOutlineInactive;
      }
    } else if (yMin >= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtr > r_ring_max_sq || dtr < r_ring_min_sq || dbl > r_ring_max_sq ||
          dbl < r_ring_min_sq) {
        return internal::arc::AreaType::kMixed;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if (arc.quadrants_ & 4 && xMax <= arc.xc - 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return internal::arc::AreaType::kOutlineActive;
      }
      if (arc.quadrants_ & 0x40 && xMax <= arc.xc - 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return internal::arc::AreaType::kOutlineInactive;
      }
    } else if (xMax <= arc.xc - arc.ri - 0.5f) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dbl > r_ring_max_sq ||
          dbl < r_ring_min_sq) {
        return internal::arc::AreaType::kMixed;
        // Fast-path for the case when the arc contains the entire half-circle.
        if ((arc.quadrants_ & 0b0101) == 0b0101) {
          return internal::arc::AreaType::kOutlineActive;
        }
      }
    } else {
      return internal::arc::AreaType::kMixed;
    }
  } else if (xMin >= arc.xc) {
    if (yMax <= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtr > r_ring_max_sq || dtr < r_ring_min_sq || dbl > r_ring_max_sq ||
          dbl < r_ring_min_sq) {
        return internal::arc::AreaType::kMixed;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if (arc.quadrants_ & 2 && xMin >= arc.xc + 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return internal::arc::AreaType::kOutlineActive;
      }
      if (arc.quadrants_ & 0x20 && xMin >= arc.xc + 0.5f &&
          yMax <= arc.yc - 0.5f) {
        return internal::arc::AreaType::kOutlineInactive;
      }
    } else if (yMin >= arc.yc) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dbr > r_ring_max_sq ||
          dbr < r_ring_min_sq) {
        return internal::arc::AreaType::kMixed;
      }
      // Fast-path for the case when the arc contains the entire quadrant.
      if (arc.quadrants_ & 8 && xMin >= arc.xc + 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return internal::arc::AreaType::kOutlineActive;
      }
      if (arc.quadrants_ & 0x80 && xMin >= arc.xc + 0.5f &&
          yMin >= arc.yc + 0.5f) {
        return internal::arc::AreaType::kOutlineInactive;
      }
    } else if (xMin >= arc.xc + arc.ri + 0.5f) {
      float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
      float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
      if (dtr > r_ring_max_sq || dtr < r_ring_min_sq || dbr > r_ring_max_sq ||
          dbr < r_ring_min_sq) {
        return internal::arc::AreaType::kMixed;
      }
      // Fast-path for the case when the arc contains the entire half-circle.
      if ((arc.quadrants_ & 0b1010) == 0b1010) {
        return internal::arc::AreaType::kOutlineActive;
      }
    } else {
      return internal::arc::AreaType::kMixed;
    }
  } else if (yMax <= arc.yc - arc.ri - 0.5f) {
    float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
    float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
    if (dtl > r_ring_max_sq || dtl < r_ring_min_sq || dtr > r_ring_max_sq ||
        dtr < r_ring_min_sq) {
      return internal::arc::AreaType::kMixed;
    }
    // Fast-path for the case when the arc contains the entire half-circle.
    if ((arc.quadrants_ & 0b0011) == 0b0011) {
      return internal::arc::AreaType::kOutlineActive;
    }
  } else if (yMin >= arc.yc + arc.ri + 0.5f) {
    float r_ring_max_sq = arc.ro_sq_adj - arc.ro;
    float r_ring_min_sq = arc.ri_sq_adj + arc.ri;
    if (dbl > r_ring_max_sq || dbl < r_ring_min_sq || dbr > r_ring_max_sq ||
        dbr < r_ring_min_sq) {
      return internal::arc::AreaType::kMixed;
    }
    // // Fast-path for the case when the arc contains the entire half-circle.
    if ((arc.quadrants_ & 0b1100) == 0b1100) {
      return internal::arc::AreaType::kOutlineActive;
    }
  } else {
    return internal::arc::AreaType::kMixed;
  }
  // The rectangle is entirely inside the ring. Let's check if it is actually
  // single-color.
  //
  // First, let's see if the rect is entirely within the 'active' angle.
  if (IsRectWithinAngle(arc.start_x_slope, arc.start_y_slope, arc.end_x_slope,
                        arc.end_y_slope, arc.range_angle_sharp, arc.xc, arc.yc,
                        box)) {
    return internal::arc::AreaType::kOutlineActive;
  }

  // Now, let's see if the rect is perhaps entirely inside the 'inactive' angle.
  if (arc.nonempty_cutoff &&
      IsRectWithinAngle(arc.end_cutoff_x_slope, arc.end_cutoff_y_slope,
                        arc.start_cutoff_x_slope, arc.start_cutoff_y_slope,
                        !arc.cutoff_angle_sharp, arc.xc, arc.yc, box)) {
    return internal::arc::AreaType::kOutlineInactive;
  }

  // Finally, check if maybe the rect is entirely within one of the round
  // endings.
  if (arc.round_endings) {
    if (IsRectWithinCircle(arc.xc + arc.start_x_rc, arc.yc + arc.start_y_rc,
                           arc.rm_sq_adj - arc.rm, box) ||
        IsRectWithinCircle(arc.xc + arc.end_x_rc, arc.yc + arc.end_y_rc,
                           arc.rm_sq_adj - arc.rm, box)) {
      return internal::arc::AreaType::kOutlineActive;
    }
  }

  // Slow case; evaluate every pixel from the rectangle.
  return internal::arc::AreaType::kMixed;
}

// Called for arcs with area <= 64 pixels.
void FillSubrectOfArc(const SmoothShape::Arc& arc, const ArcDrawSpec& spec,
                      const Box& box) {
  Color interior = arc.interior_color;
  Color outline_active = arc.outline_active_color;
  Color outline_inactive = arc.outline_inactive_color;
  switch (DetermineRectColorForArcImpl(arc, box)) {
    case internal::arc::AreaType::kExterior: {
      if (spec.fill_mode == FillMode::kExtents) {
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case internal::arc::AreaType::kInterior: {
      if (spec.fill_mode == FillMode::kExtents ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case internal::arc::AreaType::kOutlineActive: {
      if (spec.fill_mode == FillMode::kExtents ||
          outline_active != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box,
                           spec.pre_blended_outline_active);
      }
      return;
    }
    case internal::arc::AreaType::kOutlineInactive: {
      if (spec.fill_mode == FillMode::kExtents ||
          outline_inactive != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box,
                           spec.pre_blended_outline_inactive);
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

void DrawArcImpl(SmoothShape::Arc arc, const Surface& s, const Box& box) {
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

}  // namespace

namespace internal {

arc::AreaType DetermineRectColorForArc(const SmoothShape::Arc& arc,
                                       const Box& box) {
  return DetermineRectColorForArcImpl(arc, box);
}

bool ReadColorRectOfArc(const SmoothShape::Arc& arc, int16_t xMin, int16_t yMin,
                        int16_t xMax, int16_t yMax, Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForArcImpl(arc, box)) {
    case arc::AreaType::kExterior: {
      *result = color::Transparent;
      return true;
    }
    case arc::AreaType::kInterior: {
      *result = arc.interior_color;
      return true;
    }
    case arc::AreaType::kOutlineActive: {
      *result = arc.outline_active_color;
      return true;
    }
    case arc::AreaType::kOutlineInactive: {
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

void DrawArc(SmoothShape::Arc arc, const Surface& s, const Box& box) {
  DrawArcImpl(arc, s, box);
}

}  // namespace internal

}  // namespace roo_display
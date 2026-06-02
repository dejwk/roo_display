#include "roo_display/shape/impl/smooth_round_rect.h"

#include <math.h>

#include <algorithm>
#include <array>

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/shape/impl/smooth_internal.h"
#include "roo_logging.h"

namespace roo_display {

namespace {

inline float ClampNonNegative(float value) {
  return value < 0.0f ? 0.0f : value;
}

RoundRectRadii ClampRoundRectRadiiNonNegative(const RoundRectRadii& radii) {
  return {ClampNonNegative(radii.tl), ClampNonNegative(radii.tr),
          ClampNonNegative(radii.bl), ClampNonNegative(radii.br)};
}

RoundRectRadii ScaleRoundRectRadii(const RoundRectRadii& radii, float scale) {
  return {radii.tl * scale, radii.tr * scale, radii.bl * scale,
          radii.br * scale};
}

RoundRectRadii ExpandRoundRectRadii(const RoundRectRadii& radii, float delta) {
  return {radii.tl + delta, radii.tr + delta, radii.bl + delta,
          radii.br + delta};
}

RoundRectRadii InsetRoundRectRadii(const RoundRectRadii& radii, float delta) {
  return {
      ClampNonNegative(radii.tl - delta), ClampNonNegative(radii.tr - delta),
      ClampNonNegative(radii.bl - delta), ClampNonNegative(radii.br - delta)};
}

bool RoundRectRadiiAreEqual(const RoundRectRadii& radii) {
  return radii.tl == radii.tr && radii.tl == radii.bl && radii.tl == radii.br;
}

void TightenScaleLimit(float available, float required, float* scale) {
  if (required <= 0.0f) return;
  const float axis_scale = available / required;
  if (axis_scale < *scale) {
    *scale = axis_scale;
  }
}

constexpr int kTopLeftCorner = 0;
constexpr int kTopRightCorner = 1;
constexpr int kBottomLeftCorner = 2;
constexpr int kBottomRightCorner = 3;
constexpr float kDiagInsetFactor = 1.0f - 0.7071067811865476f;

inline float ClampUnitInterval(float value) {
  if (value <= 0.0f) return 0.0f;
  if (value >= 1.0f) return 1.0f;
  return value;
}

void CopyRoundRectRadii(const RoundRectRadii& radii, float out[4]) {
  out[kTopLeftCorner] = radii.tl;
  out[kTopRightCorner] = radii.tr;
  out[kBottomLeftCorner] = radii.bl;
  out[kBottomRightCorner] = radii.br;
}

inline float CornerCenterX(float x0, float x1, const float radii[4],
                           int corner) {
  return corner == kTopLeftCorner || corner == kBottomLeftCorner
             ? x0 + radii[corner]
             : x1 - radii[corner];
}

inline float CornerCenterY(float y0, float y1, const float radii[4],
                           int corner) {
  return corner == kTopLeftCorner || corner == kTopRightCorner
             ? y0 + radii[corner]
             : y1 - radii[corner];
}

inline float SafeCircularOffset(float radius, float delta) {
  if (radius <= 0.0f) return 0.0f;
  const float remainder = radius * radius - delta * delta;
  return remainder <= 0.0f ? 0.0f : sqrtf(remainder);
}

Box ToInteriorHelperBox(float x0, float y0, float x1, float y1) {
  return Box((int16_t)ceilf(x0 + 0.5f), (int16_t)ceilf(y0 + 0.5f),
             (int16_t)floorf(x1 - 0.5f), (int16_t)floorf(y1 - 0.5f));
}

struct RoundRectCornersBuildResult {
  Box extents;
  SmoothShape::RoundRectCorners spec;
};

RoundRectCornersBuildResult BuildRoundRectCornersSpec(
    float x0, float y0, float x1, float y1, const RoundRectRadii& outer_radii,
    float thickness, Color outline_color, Color interior_color) {
  Box extents((int16_t)floorf(x0 + 0.5f), (int16_t)floorf(y0 + 0.5f),
              (int16_t)ceilf(x1 - 0.5f), (int16_t)ceilf(y1 - 0.5f));

  float ro[4];
  float ri[4];
  float ro_sq_adj[4];
  float ri_sq_adj[4];
  CopyRoundRectRadii(outer_radii, ro);
  for (int corner = 0; corner < 4; ++corner) {
    ri[corner] = ClampNonNegative(ro[corner] - thickness);
    ro_sq_adj[corner] = ro[corner] * ro[corner] + 0.25f;
    ri_sq_adj[corner] = ri[corner] * ri[corner] + 0.25f;
  }

  const float inner_x0 = x0 + thickness;
  const float inner_y0 = y0 + thickness;
  const float inner_x1 = x1 - thickness;
  const float inner_y1 = y1 - thickness;

  const float ctl_x = inner_x0 + ri[kTopLeftCorner];
  const float ctl_y = inner_y0 + ri[kTopLeftCorner];
  const float ctr_x = inner_x1 - ri[kTopRightCorner];
  const float ctr_y = inner_y0 + ri[kTopRightCorner];
  const float cbl_x = inner_x0 + ri[kBottomLeftCorner];
  const float cbl_y = inner_y1 - ri[kBottomLeftCorner];
  const float cbr_x = inner_x1 - ri[kBottomRightCorner];
  const float cbr_y = inner_y1 - ri[kBottomRightCorner];

  const auto top_inside = [&](float x) {
    float result = inner_y0;
    if (x < ctl_x && ri[kTopLeftCorner] > 0.0f) {
      result = std::max(
          result, ctl_y - SafeCircularOffset(ri[kTopLeftCorner], x - ctl_x));
    }
    if (x > ctr_x && ri[kTopRightCorner] > 0.0f) {
      result = std::max(
          result, ctr_y - SafeCircularOffset(ri[kTopRightCorner], x - ctr_x));
    }
    return result;
  };

  const auto bottom_inside = [&](float x) {
    float result = inner_y1;
    if (x < cbl_x && ri[kBottomLeftCorner] > 0.0f) {
      result = std::min(
          result, cbl_y + SafeCircularOffset(ri[kBottomLeftCorner], x - cbl_x));
    }
    if (x > cbr_x && ri[kBottomRightCorner] > 0.0f) {
      result = std::min(result, cbr_y + SafeCircularOffset(
                                            ri[kBottomRightCorner], x - cbr_x));
    }
    return result;
  };

  const auto left_inside = [&](float y) {
    float result = inner_x0;
    if (y < ctl_y && ri[kTopLeftCorner] > 0.0f) {
      result = std::max(
          result, ctl_x - SafeCircularOffset(ri[kTopLeftCorner], y - ctl_y));
    }
    if (y > cbl_y && ri[kBottomLeftCorner] > 0.0f) {
      result = std::max(
          result, cbl_x - SafeCircularOffset(ri[kBottomLeftCorner], y - cbl_y));
    }
    return result;
  };

  const auto right_inside = [&](float y) {
    float result = inner_x1;
    if (y < ctr_y && ri[kTopRightCorner] > 0.0f) {
      result = std::min(
          result, ctr_x + SafeCircularOffset(ri[kTopRightCorner], y - ctr_y));
    }
    if (y > cbr_y && ri[kBottomRightCorner] > 0.0f) {
      result = std::min(result, cbr_x + SafeCircularOffset(
                                            ri[kBottomRightCorner], y - cbr_y));
    }
    return result;
  };

  const float diag_tl = ri[kTopLeftCorner] * kDiagInsetFactor;
  const float diag_tr = ri[kTopRightCorner] * kDiagInsetFactor;
  const float diag_bl = ri[kBottomLeftCorner] * kDiagInsetFactor;
  const float diag_br = ri[kBottomRightCorner] * kDiagInsetFactor;

  const Box top_slab =
      ToInteriorHelperBox(ctl_x, inner_y0, ctr_x,
                          std::min(bottom_inside(ctl_x), bottom_inside(ctr_x)));
  const Box bottom_slab = ToInteriorHelperBox(
      cbl_x, std::max(top_inside(cbl_x), top_inside(cbr_x)), cbr_x, inner_y1);
  const Box left_slab = ToInteriorHelperBox(
      inner_x0, ctl_y, std::min(right_inside(ctl_y), right_inside(cbl_y)),
      cbl_y);
  const Box right_slab = ToInteriorHelperBox(
      std::max(left_inside(ctr_y), left_inside(cbr_y)), ctr_y, inner_x1, cbr_y);
  const Box inner_core =
      ToInteriorHelperBox(inner_x0 + std::max(diag_tl, diag_bl),
                          inner_y0 + std::max(diag_tl, diag_tr),
                          inner_x1 - std::max(diag_tr, diag_br),
                          inner_y1 - std::max(diag_bl, diag_br));

  SmoothShape::RoundRectCorners spec{
      x0,
      y0,
      x1,
      y1,
      {ro[kTopLeftCorner], ro[kTopRightCorner], ro[kBottomLeftCorner],
       ro[kBottomRightCorner]},
      thickness,
      {ro_sq_adj[kTopLeftCorner], ro_sq_adj[kTopRightCorner],
       ro_sq_adj[kBottomLeftCorner], ro_sq_adj[kBottomRightCorner]},
      {ri_sq_adj[kTopLeftCorner], ri_sq_adj[kTopRightCorner],
       ri_sq_adj[kBottomLeftCorner], ri_sq_adj[kBottomRightCorner]},
      outline_color,
      interior_color,
      inner_core,
      top_slab,
      bottom_slab,
      left_slab,
      right_slab};
  return {std::move(extents), std::move(spec)};
}

}  // namespace

namespace internal {

NormalizedSingleRadiusRoundRect NormalizeSingleRadiusRoundRect(
    float x0, float y0, float x1, float y1, float radius, float thickness) {
  if (radius < 0.0f) radius = 0.0f;
  if (thickness < 0.0f) thickness = 0.0f;
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);

  float width = x1 - x0;
  float height = y1 - y0;
  float max_radius = ((width < height) ? width : height) * 0.5f;
  if (radius > max_radius) radius = max_radius;

  // Normalize everything up front so later code can assume canonical ordering,
  // non-negative thickness, and a valid inner boundary description.
  float delta = thickness * 0.5f;
  float inner_x0 = x0 + delta;
  float inner_y0 = y0 + delta;
  float inner_x1 = x1 - delta;
  float inner_y1 = y1 - delta;
  float inner_radius = radius - delta;
  if (inner_radius < 0.0f) inner_radius = 0.0f;

  NormalizedRoundRectKind kind =
      (inner_x0 >= inner_x1 || inner_y0 >= inner_y1)
          ? NormalizedRoundRectKind::kFilled
          : (inner_radius > 0.0f ? NormalizedRoundRectKind::kRoundInner
                                 : NormalizedRoundRectKind::kRectInner);

  return {kind,       x0 - delta,     y0 - delta,  x1 + delta,
          y1 + delta, radius + delta, inner_x0,    inner_y0,
          inner_x1,   inner_y1,       inner_radius};
}

NormalizedFourRadiiRoundRect NormalizeFourRadiiRoundRect(
    float x0, float y0, float x1, float y1, const RoundRectRadii& radii,
    float thickness) {
  thickness = ClampNonNegative(thickness);
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);

  const RoundRectRadii clamped_radii = ClampRoundRectRadiiNonNegative(radii);
  const float width = x1 - x0;
  const float height = y1 - y0;
  float scale = 1.0f;
  TightenScaleLimit(width, clamped_radii.tl + clamped_radii.tr, &scale);
  TightenScaleLimit(width, clamped_radii.bl + clamped_radii.br, &scale);
  TightenScaleLimit(height, clamped_radii.tl + clamped_radii.bl, &scale);
  TightenScaleLimit(height, clamped_radii.tr + clamped_radii.br, &scale);

  const RoundRectRadii centerline_radii =
      ScaleRoundRectRadii(clamped_radii, scale);
  const float delta = thickness * 0.5f;
  const float inner_x0 = x0 + delta;
  const float inner_y0 = y0 + delta;
  const float inner_x1 = x1 - delta;
  const float inner_y1 = y1 - delta;

  return {inner_x0 >= inner_x1 || inner_y0 >= inner_y1,
          RoundRectRadiiAreEqual(centerline_radii),
          x0,
          y0,
          x1,
          y1,
          thickness,
          x0 - delta,
          y0 - delta,
          x1 + delta,
          y1 + delta,
          inner_x0,
          inner_y0,
          inner_x1,
          inner_y1,
          centerline_radii,
          ExpandRoundRectRadii(centerline_radii, delta),
          InsetRoundRectRadii(centerline_radii, delta)};
}

}  // namespace internal

SmoothShape SmoothThickRoundRect(float x0, float y0, float x1, float y1,
                                 float radius, float thickness, Color color,
                                 Color interior_color) {
  if (thickness < 0) thickness = 0;
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(x0, y0, x1, y1, radius,
                                               thickness);
  const bool rectangular_inner =
      normalized.kind == internal::NormalizedRoundRectKind::kRectInner;
  if (normalized.kind == internal::NormalizedRoundRectKind::kFilled) {
    thickness = 0.0f;
    interior_color = color;
  }
  if (thickness == 0.0f) {
    color = interior_color;
  }

  x0 = normalized.outer_x0;
  y0 = normalized.outer_y0;
  x1 = normalized.outer_x1;
  y1 = normalized.outer_y1;
  radius = normalized.outer_radius;
  float width = x1 - x0;
  float height = y1 - y0;
  float outer_radius = radius;
  if (outer_radius < 0) outer_radius = 0;
  float inner_radius = outer_radius - thickness;
  if (inner_radius < 0) inner_radius = 0;
  const float inner_total_width =
      std::max(0.0f, normalized.inner_x1 - normalized.inner_x0);
  const float inner_total_height =
      std::max(0.0f, normalized.inner_y1 - normalized.inner_y0);
  Box extents((int16_t)floorf(x0 + 0.5f), (int16_t)floorf(y0 + 0.5f),
              (int16_t)ceilf(x1 - 0.5f), (int16_t)ceilf(y1 - 0.5f));
  if (extents.xMin() == extents.xMax() && extents.yMin() == extents.yMax()) {
    // Special case: the rect fits into a single pixel. Let's just calculate the
    // color and get it over with.
    float outer_area = std::min<float>(
        1.0f,
        std::max<float>(0.0f, width * height -
                                  (4.0f - M_PI) * outer_radius * outer_radius));
    float inner_area = std::min<float>(
        1.0f,
        std::max<float>(0.0f, inner_total_width * inner_total_height -
                                  (4.0f - M_PI) * inner_radius * inner_radius));
    color = color.withA(color.a() * outer_area);
    interior_color = interior_color.withA(interior_color.a() * inner_area);
    Color pixel_color = AlphaBlend(color, interior_color);
    if (pixel_color.a() == 0) return SmoothShape();
    return SmoothShape(extents.xMin(), extents.yMin(),
                       SmoothShape::Pixel{color});
  }

  x0 += outer_radius;
  y0 += outer_radius;
  x1 -= outer_radius;
  y1 -= outer_radius;
  const SmoothShape::RoundRect::InnerBoundaryMode inner_boundary_mode =
      rectangular_inner ? SmoothShape::RoundRect::InnerBoundaryMode::kRect
                        : SmoothShape::RoundRect::InnerBoundaryMode::kRound;
  Box inner_mid;
  Box inner_wide;
  Box inner_tall;
  // Precompute conservative interior boxes so the pixel, tile, and readback
  // paths can recognize obvious interior regions without distance math.
  if (rectangular_inner) {
    const Box inner_rect_interior((int16_t)ceilf(normalized.inner_x0 + 0.5f),
                                  (int16_t)ceilf(normalized.inner_y0 + 0.5f),
                                  (int16_t)floorf(normalized.inner_x1 - 0.5f),
                                  (int16_t)floorf(normalized.inner_y1 - 0.5f));
    inner_mid = inner_rect_interior;
    inner_wide = inner_rect_interior;
    inner_tall = inner_rect_interior;
  } else {
    float diagonal_offset = sqrtf(0.5f) * inner_radius;
    inner_mid = Box((int16_t)ceilf(x0 - diagonal_offset + 0.5f),
                    (int16_t)ceilf(y0 - diagonal_offset + 0.5f),
                    (int16_t)floorf(x1 + diagonal_offset - 0.5f),
                    (int16_t)floorf(y1 + diagonal_offset - 0.5f));
    inner_wide = Box(
        (int16_t)ceilf(x0 - inner_radius + 0.5f), (int16_t)ceilf(y0 + 0.5f),
        (int16_t)floorf(x1 + inner_radius - 0.5f), (int16_t)floorf(y1 - 0.5f));
    inner_tall = Box(
        (int16_t)ceilf(x0 + 0.5f), (int16_t)ceilf(y0 - inner_radius + 0.5f),
        (int16_t)floorf(x1 - 0.5f), (int16_t)floorf(y1 + inner_radius - 0.5f));
  }
  SmoothShape::RoundRect spec{x0,
                              y0,
                              x1,
                              y1,
                              outer_radius,
                              inner_radius,
                              outer_radius * outer_radius + 0.25f,
                              inner_radius * inner_radius + 0.25f,
                              normalized.inner_x0,
                              normalized.inner_y0,
                              normalized.inner_x1,
                              normalized.inner_y1,
                              color,
                              interior_color,
                              std::move(inner_mid),
                              std::move(inner_wide),
                              std::move(inner_tall),
                              inner_boundary_mode};

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

SmoothShape SmoothThickRoundRect(float x0, float y0, float x1, float y1,
                                 const RoundRectRadii& radii, float thickness,
                                 Color color, Color interior_color) {
  const internal::NormalizedFourRadiiRoundRect normalized =
      internal::NormalizeFourRadiiRoundRect(x0, y0, x1, y1, radii, thickness);
  if (normalized.filled) {
    if (normalized.has_equal_centerline_radii) {
      return SmoothFilledRoundRect(normalized.outer_x0, normalized.outer_y0,
                                   normalized.outer_x1, normalized.outer_y1,
                                   normalized.outer_radii.tl, color);
    }
    RoundRectCornersBuildResult built = BuildRoundRectCornersSpec(
        normalized.outer_x0, normalized.outer_y0, normalized.outer_x1,
        normalized.outer_y1, normalized.outer_radii, 0.0f, color, color);
    if (built.extents.empty()) {
      return SmoothShape();
    }
    return SmoothShape(std::move(built.extents), std::move(built.spec));
  }
  if (normalized.has_equal_centerline_radii) {
    return SmoothThickRoundRect(
        normalized.centerline_x0, normalized.centerline_y0,
        normalized.centerline_x1, normalized.centerline_y1,
        normalized.centerline_radii.tl, normalized.thickness, color,
        interior_color);
  }
  RoundRectCornersBuildResult built = BuildRoundRectCornersSpec(
      normalized.outer_x0, normalized.outer_y0, normalized.outer_x1,
      normalized.outer_y1, normalized.outer_radii, normalized.thickness, color,
      interior_color);
  if (built.extents.empty()) {
    return SmoothShape();
  }
  return SmoothShape(std::move(built.extents), std::move(built.spec));
}

SmoothShape SmoothRoundRect(float x0, float y0, float x1, float y1,
                            const RoundRectRadii& radii, Color color,
                            Color interior_color) {
  return SmoothThickRoundRect(x0, y0, x1, y1, radii, 1.0f, color,
                              interior_color);
}

SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                                  const RoundRectRadii& radii, Color color) {
  return SmoothThickRoundRect(x0, y0, x1, y1, radii, 0.0f, color, color);
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

inline bool UsesRectInnerBoundary(const SmoothShape::RoundRect& rect) {
  return rect.inner_boundary_mode ==
         SmoothShape::RoundRect::InnerBoundaryMode::kRect;
}

inline bool PointInsideRoundRectInteriorHelper(
    const SmoothShape::RoundRect& rect, int16_t x, int16_t y) {
  return rect.inner_mid.contains(x, y) || rect.inner_wide.contains(x, y) ||
         rect.inner_tall.contains(x, y);
}

inline bool BoxInsideRoundRectInteriorHelper(const SmoothShape::RoundRect& rect,
                                             const Box& box) {
  return rect.inner_mid.contains(box) || rect.inner_wide.contains(box) ||
         rect.inner_tall.contains(box);
}

inline bool BoxInsideRoundRectCornersInteriorHelper(
    const SmoothShape::RoundRectCorners& rect, const Box& box) {
  return rect.inner_core.contains(box) || rect.top_slab.contains(box) ||
         rect.bottom_slab.contains(box) || rect.left_slab.contains(box) ||
         rect.right_slab.contains(box);
}

inline bool PointInsideRoundRectCornersInteriorHelper(
    const SmoothShape::RoundRectCorners& rect, int16_t x, int16_t y) {
  return rect.inner_core.contains(x, y) || rect.top_slab.contains(x, y) ||
         rect.bottom_slab.contains(x, y) || rect.left_slab.contains(x, y) ||
         rect.right_slab.contains(x, y);
}

inline float CalcDistSqRectInnerBoundary(const SmoothShape::RoundRect& rect,
                                         float xt, float yt);

inline float CalcDistSqRoundRectInnerBoundary(
    const SmoothShape::RoundRect& rect, float outer_d_squared, int16_t x,
    int16_t y) {
  return UsesRectInnerBoundary(rect) ? CalcDistSqRectInnerBoundary(rect, x, y)
                                     : outer_d_squared;
}

inline Color GetSmoothRoundRectPixelColor(const SmoothShape::RoundRect& rect,
                                          int16_t x, int16_t y) {
  const bool uses_rect_inner = UsesRectInnerBoundary(rect);
  if (rect.ri < 0.5) {
    // Note: checking this unconditionally is correct, but since it tends to
    // apply to few pixels, it is not a net win. But for ri < 0.5, it is needed
    // for correctness, due to the math below.
    if (PointInsideRoundRectInteriorHelper(rect, x, y)) {
      return rect.interior_color;
    }
  }
  // Project the pixel onto the rectangle's center slab first; only the corner
  // residual contributes to the circular distance checks.
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

  // Most pixels fall cleanly into one of these three buckets, so check the
  // no-sqrt cases first and leave anti-aliasing for the narrow boundary band.
  if (!uses_rect_inner && d_squared <= rect.ri_sq_adj - ri && ri >= 0.5f) {
    // Point fully within the interior.
    return interior;
  }
  if (d_squared >= rect.ro_sq_adj + ro) {
    // Point fully outside the round rectangle.
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= rect.ro_sq_adj - ro;
  float inner_d_squared =
      CalcDistSqRoundRectInnerBoundary(rect, d_squared, x, y);
  bool fully_outside_inner = ro == ri || inner_d_squared >= rect.ri_sq_adj + ri;
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
  float outer_d = sqrtf(d_squared);
  if (fully_outside_inner) {
    // On the outer boundary.
    return outline.withA((uint8_t)(outline.a() * (ro - outer_d + 0.5f)));
  }
  float inner_d = uses_rect_inner ? sqrtf(inner_d_squared) : outer_d;
  if (fully_within_outer) {
    // On the inner boundary.
    float outline_coverage = 1.0f - (ri - inner_d + 0.5f);
    if (!uses_rect_inner && ri < 0.5f &&
        (roundf(rect.x0 - ri) == roundf(rect.x1 + ri) ||
         roundf(rect.y0 - ri) == roundf(rect.y1 + ri))) {
      // Pesky corner case: the interior is hair-thin. Approximate.
      outline_coverage = std::min(1.0f, outline_coverage * 2.0f);
    }
    const uint16_t outline_fraction = outline_coverage * 256;
    return InterpolateColors(interior, outline, outline_fraction);
  }
  // On both bounderies (e.g. the band is very thin).
  const uint16_t outer_fraction = (ro - outer_d + 0.5f) * 256;
  const uint16_t interior_fraction = (ri - inner_d + 0.5f) * 256;
  const uint16_t outline_fraction = outer_fraction - interior_fraction;
  return MixColors(interior, interior_fraction, outline, outline_fraction);
}

// Squared distance to an axis-aligned rectangle. This is the cheap no-sqrt
// primitive used by the straight-edge inner-boundary checks.
inline float CalcDistSqRect(float x0, float y0, float x1, float y1, float xt,
                            float yt) {
  float dx = (xt <= x0 ? xt - x0 : xt >= x1 ? xt - x1 : 0.0f);
  float dy = (yt <= y0 ? yt - y0 : yt >= y1 ? yt - y1 : 0.0f);
  return dx * dx + dy * dy;
}

// Fast containment check for the straight-edged portion of the inner boundary.
inline bool PointInsideRect(float x0, float y0, float x1, float y1, float xt,
                            float yt) {
  return xt >= x0 && xt <= x1 && yt >= y0 && yt <= y1;
}

// Continuous-box proofs against an axis-aligned edge use the tile expanded by
// +/- 0.5 pixels, matching the AA thresholds used by the per-pixel evaluator.
inline bool BoxFullyInsideRect(float x0, float y0, float x1, float y1,
                               float x_min, float y_min, float x_max,
                               float y_max) {
  return x_min >= x0 && x_max <= x1 && y_min >= y0 && y_max <= y1;
}

inline bool BoxFullyOutsideRect(float x0, float y0, float x1, float y1,
                                float x_min, float y_min, float x_max,
                                float y_max) {
  return x_max <= x0 || x_min >= x1 || y_max <= y0 || y_min >= y1;
}

// Minimum inset from a point already known to be inside the rectangle.
inline float DistToNearestRectEdgeFromInside(float x0, float y0, float x1,
                                             float y1, float xt, float yt) {
  return std::min(std::min(xt - x0, x1 - xt), std::min(yt - y0, y1 - yt));
}

// Signed distance to the straight slab of the round rect. Positive means the
// pixel center is inside the slab, negative means it lies outside it.
inline float SignedDistanceToRectStraightEdge(float x0, float y0, float x1,
                                              float y1, float xt, float yt) {
  if (xt < x0) return xt - x0;
  if (xt > x1) return x1 - xt;
  if (yt < y0) return yt - y0;
  if (yt > y1) return y1 - yt;
  return DistToNearestRectEdgeFromInside(x0, y0, x1, y1, xt, yt);
}

// Returns the rounded-corner quadrant that owns this pixel. A `-1` result
// means the pixel lies on one of the straight slabs, so the corner-circle math
// can be skipped entirely.
inline int SelectRoundedCorner(float xt, float yt, float x0, float y0, float x1,
                               float y1, const float radii[4]) {
  if (radii[kTopLeftCorner] > 0.0f && xt < x0 + radii[kTopLeftCorner] &&
      yt < y0 + radii[kTopLeftCorner]) {
    return kTopLeftCorner;
  }
  if (radii[kTopRightCorner] > 0.0f && xt > x1 - radii[kTopRightCorner] &&
      yt < y0 + radii[kTopRightCorner]) {
    return kTopRightCorner;
  }
  if (radii[kBottomLeftCorner] > 0.0f && xt < x0 + radii[kBottomLeftCorner] &&
      yt > y1 - radii[kBottomLeftCorner]) {
    return kBottomLeftCorner;
  }
  if (radii[kBottomRightCorner] > 0.0f && xt > x1 - radii[kBottomRightCorner] &&
      yt > y1 - radii[kBottomRightCorner]) {
    return kBottomRightCorner;
  }
  return -1;
}

// Whole-tile version of `SelectRoundedCorner()`. A non-negative result proves
// the entire tile lives in one corner quadrant, which lets the classifier use
// only corner-local squared-distance bounds.
inline int SelectRoundedCornerBox(float x_min, float y_min, float x_max,
                                  float y_max, float x0, float y0, float x1,
                                  float y1, const float radii[4]) {
  if (radii[kTopLeftCorner] > 0.0f && x_max <= x0 + radii[kTopLeftCorner] &&
      y_max <= y0 + radii[kTopLeftCorner]) {
    return kTopLeftCorner;
  }
  if (radii[kTopRightCorner] > 0.0f && x_min >= x1 - radii[kTopRightCorner] &&
      y_max <= y0 + radii[kTopRightCorner]) {
    return kTopRightCorner;
  }
  if (radii[kBottomLeftCorner] > 0.0f &&
      x_max <= x0 + radii[kBottomLeftCorner] &&
      y_min >= y1 - radii[kBottomLeftCorner]) {
    return kBottomLeftCorner;
  }
  if (radii[kBottomRightCorner] > 0.0f &&
      x_min >= x1 - radii[kBottomRightCorner] &&
      y_min >= y1 - radii[kBottomRightCorner]) {
    return kBottomRightCorner;
  }
  return -1;
}

struct CornerDistanceSqRange {
  float nearest_sq;
  float farthest_sq;
};

// In a single quadrant, the nearest and farthest distances occur at opposite
// rectangle corners, so the classifier can bound the whole tile with two
// squared-distance checks.
inline CornerDistanceSqRange CalcCornerDistanceSqRange(float x_min, float y_min,
                                                       float x_max, float y_max,
                                                       float cx, float cy,
                                                       int corner) {
  const float near_x =
      corner == kTopLeftCorner || corner == kBottomLeftCorner ? x_max : x_min;
  const float far_x =
      corner == kTopLeftCorner || corner == kBottomLeftCorner ? x_min : x_max;
  const float near_y =
      corner == kTopLeftCorner || corner == kTopRightCorner ? y_max : y_min;
  const float far_y =
      corner == kTopLeftCorner || corner == kTopRightCorner ? y_min : y_max;

  const float near_dx = near_x - cx;
  const float near_dy = near_y - cy;
  const float far_dx = far_x - cx;
  const float far_dy = far_y - cy;
  return {near_dx * near_dx + near_dy * near_dy,
          far_dx * far_dx + far_dy * far_dy};
}

// Converts coverage in pixel units to the 0..256 blending fractions expected
// by `InterpolateColors()` and `MixColors()`.
inline uint16_t CoverageToFraction(float coverage) {
  return ClampUnitInterval(coverage) * 256.0f;
}

// Mirrors the single-radius evaluator's strategy: prove the obvious cases with
// cheap slab tests and cached squared-radius thresholds first, then fall back
// to sqrt only for pixels that land in the antialiased fringe.
inline Color GetSmoothRoundRectCornersPixelColor(
    const SmoothShape::RoundRectCorners& rect, int16_t x, int16_t y) {
  const float xt = x;
  const float yt = y;
  const float inner_x0 = rect.x0 + rect.thickness;
  const float inner_y0 = rect.y0 + rect.thickness;
  const float inner_x1 = rect.x1 - rect.thickness;
  const float inner_y1 = rect.y1 - rect.thickness;
  const float inner_radii[4] = {
      ClampNonNegative(rect.ro[kTopLeftCorner] - rect.thickness),
      ClampNonNegative(rect.ro[kTopRightCorner] - rect.thickness),
      ClampNonNegative(rect.ro[kBottomLeftCorner] - rect.thickness),
      ClampNonNegative(rect.ro[kBottomRightCorner] - rect.thickness),
  };

  const int outer_corner =
      SelectRoundedCorner(xt, yt, rect.x0, rect.y0, rect.x1, rect.y1, rect.ro);
  bool fully_within_outer;
  float outer_d_squared = 0.0f;
  float outer_distance = 0.0f;
  float outer_radius = 0.0f;
  float outer_coverage = 1.0f;
  if (outer_corner < 0) {
    // Away from the rounded corners, the extents contract means this can only
    // be the straight-edge body or its AA fringe. Clamp keeps the helper
    // benign if a caller ever violates that contract.
    outer_coverage =
        ClampNonNegative(SignedDistanceToRectStraightEdge(
                             rect.x0, rect.y0, rect.x1, rect.y1, xt, yt) +
                         0.5f);
    fully_within_outer = outer_coverage >= 1.0f;
  } else {
    // Rounded corner: use the cached `r^2 + 0.25` thresholds so most pixels
    // classify without taking a square root.
    outer_radius = rect.ro[outer_corner];
    const float outer_dx =
        xt - CornerCenterX(rect.x0, rect.x1, rect.ro, outer_corner);
    const float outer_dy =
        yt - CornerCenterY(rect.y0, rect.y1, rect.ro, outer_corner);
    outer_d_squared = outer_dx * outer_dx + outer_dy * outer_dy;
    if (outer_d_squared >= rect.ro_sq_adj[outer_corner] + outer_radius) {
      return color::Transparent;
    }
    fully_within_outer =
        outer_d_squared <= rect.ro_sq_adj[outer_corner] - outer_radius;
  }

  const bool rounded_inner_corner =
      outer_corner >= 0 && inner_radii[outer_corner] > 0.0f;
  float inner_d_squared = 0.0f;
  float inner_distance = 0.0f;
  float inner_radius = 0.0f;
  bool fully_outside_inner;

  if (rounded_inner_corner) {
    // Insetting by the outline thickness preserves the owning corner: once the
    // outer boundary classified this pixel into a corner quadrant, the inner
    // rounded boundary is either the same corner or it has collapsed away.
    inner_radius = inner_radii[outer_corner];
    const float inner_dx =
        xt - CornerCenterX(inner_x0, inner_x1, inner_radii, outer_corner);
    const float inner_dy =
        yt - CornerCenterY(inner_y0, inner_y1, inner_radii, outer_corner);
    inner_d_squared = inner_dx * inner_dx + inner_dy * inner_dy;
    if (inner_radius >= 0.5f &&
        inner_d_squared <= rect.ri_sq_adj[outer_corner] - inner_radius) {
      return rect.interior_color;
    }
    fully_outside_inner =
        inner_d_squared >= rect.ri_sq_adj[outer_corner] + inner_radius;
  } else {
    // Once an inner corner collapses, the inner boundary in that quadrant is a
    // sharp rectangle corner, so switch to the straight-edge proof.
    if (PointInsideRect(inner_x0, inner_y0, inner_x1, inner_y1, xt, yt) &&
        DistToNearestRectEdgeFromInside(inner_x0, inner_y0, inner_x1, inner_y1,
                                        xt, yt) >= 0.5f) {
      return rect.interior_color;
    }
    inner_d_squared =
        CalcDistSqRect(inner_x0, inner_y0, inner_x1, inner_y1, xt, yt);
    fully_outside_inner = inner_d_squared >= 0.25f;
  }

  if (fully_within_outer && fully_outside_inner) {
    // The pixel is cleanly inside the outline band.
    return rect.outline_color;
  }

  if (!fully_within_outer) {
    if (outer_corner >= 0) {
      // We only pay for sqrt on the outer edge when the cheap tests could not
      // prove the pixel fully inside or fully outside that boundary.
      outer_distance = sqrtf(outer_d_squared);
      outer_coverage = outer_radius - outer_distance + 0.5f;
    }
    if (fully_outside_inner) {
      // Only the outer AA fringe contributes.
      return rect.outline_color.withA(rect.outline_color.a() * outer_coverage);
    }
  }

  if (inner_d_squared > 0.0f) {
    // Inner sqrt is likewise reserved for the narrow AA band around the inner
    // boundary.
    inner_distance = sqrtf(inner_d_squared);
  }
  if (fully_within_outer) {
    // Only the inner boundary is antialiased here.
    const uint16_t outline_fraction =
        CoverageToFraction(1.0f - (inner_radius - inner_distance + 0.5f));
    return InterpolateColors(rect.interior_color, rect.outline_color,
                             outline_fraction);
  }
  // Both boundaries are partially covering the pixel; mix the surviving
  // interior and outline fractions.
  const uint16_t outer_fraction = CoverageToFraction(outer_coverage);
  const uint16_t interior_fraction = std::min<uint16_t>(
      outer_fraction, CoverageToFraction(inner_radius - inner_distance + 0.5f));
  const uint16_t outline_fraction = outer_fraction - interior_fraction;
  return MixColors(rect.interior_color, interior_fraction, rect.outline_color,
                   outline_fraction);
}

// Corner tiles mirror the point evaluator's early returns: helper-box accept,
// then outer-corner reject/full-cover proof, then the inner-boundary proof for
// the same owning corner or its collapsed rectangular replacement.
inline internal::round_rect::AreaType
DetermineRectColorForRoundRectCornersInCorner(
    const SmoothShape::RoundRectCorners& rect, float x_min, float y_min,
    float x_max, float y_max, float inner_x0, float inner_y0, float inner_x1,
    float inner_y1, const float inner_radii[4], int outer_corner) {
  const float outer_radius = rect.ro[outer_corner];
  const CornerDistanceSqRange outer_range = CalcCornerDistanceSqRange(
      x_min, y_min, x_max, y_max,
      CornerCenterX(rect.x0, rect.x1, rect.ro, outer_corner),
      CornerCenterY(rect.y0, rect.y1, rect.ro, outer_corner), outer_corner);
  if (outer_range.nearest_sq >= rect.ro_sq_adj[outer_corner] + outer_radius) {
    return internal::round_rect::AreaType::kExterior;
  }
  if (outer_range.farthest_sq > rect.ro_sq_adj[outer_corner] - outer_radius) {
    return internal::round_rect::AreaType::kMixed;
  }

  const float inner_radius = inner_radii[outer_corner];
  if (inner_radius <= 0.0f) {
    if (BoxFullyInsideRect(inner_x0, inner_y0, inner_x1, inner_y1, x_min, y_min,
                           x_max, y_max)) {
      return internal::round_rect::AreaType::kInterior;
    }
    if (BoxFullyOutsideRect(inner_x0, inner_y0, inner_x1, inner_y1, x_min,
                            y_min, x_max, y_max)) {
      return internal::round_rect::AreaType::kOutline;
    }
    return internal::round_rect::AreaType::kMixed;
  }

  const CornerDistanceSqRange inner_range = CalcCornerDistanceSqRange(
      x_min, y_min, x_max, y_max,
      CornerCenterX(inner_x0, inner_x1, inner_radii, outer_corner),
      CornerCenterY(inner_y0, inner_y1, inner_radii, outer_corner),
      outer_corner);
  if (inner_radius >= 0.5f &&
      inner_range.farthest_sq <= rect.ri_sq_adj[outer_corner] - inner_radius) {
    return internal::round_rect::AreaType::kInterior;
  }
  if (inner_range.nearest_sq >= rect.ri_sq_adj[outer_corner] + inner_radius) {
    return internal::round_rect::AreaType::kOutline;
  }
  return internal::round_rect::AreaType::kMixed;
}

// Straight-edge tiles use the same early-return shape as the point evaluator:
// first prove the tile fully inside the outer edge, then prove it stays clear
// of the inner edge. Anything that reaches a curved transition remains mixed.
inline internal::round_rect::AreaType
DetermineRectColorForRoundRectCornersStraightBand(
    const SmoothShape::RoundRectCorners& rect, float x_min, float y_min,
    float x_max, float y_max, float inner_x0, float inner_y0, float inner_x1,
    float inner_y1, const float inner_radii[4]) {
  const float outer_top_x0 = rect.x0 + rect.ro[kTopLeftCorner];
  const float outer_top_x1 = rect.x1 - rect.ro[kTopRightCorner];
  if (x_min >= outer_top_x0 && x_max <= outer_top_x1) {
    if (y_min < rect.y0) return internal::round_rect::AreaType::kMixed;
    const float inner_top_x0 = inner_x0 + inner_radii[kTopLeftCorner];
    const float inner_top_x1 = inner_x1 - inner_radii[kTopRightCorner];
    if (x_min < inner_top_x0 || x_max > inner_top_x1) {
      return internal::round_rect::AreaType::kMixed;
    }
    if (y_max <= inner_y0) return internal::round_rect::AreaType::kOutline;
    return internal::round_rect::AreaType::kMixed;
  }

  const float outer_bottom_x0 = rect.x0 + rect.ro[kBottomLeftCorner];
  const float outer_bottom_x1 = rect.x1 - rect.ro[kBottomRightCorner];
  if (x_min >= outer_bottom_x0 && x_max <= outer_bottom_x1) {
    if (y_max > rect.y1) return internal::round_rect::AreaType::kMixed;
    const float inner_bottom_x0 = inner_x0 + inner_radii[kBottomLeftCorner];
    const float inner_bottom_x1 = inner_x1 - inner_radii[kBottomRightCorner];
    if (x_min < inner_bottom_x0 || x_max > inner_bottom_x1) {
      return internal::round_rect::AreaType::kMixed;
    }
    if (y_min >= inner_y1) return internal::round_rect::AreaType::kOutline;
    return internal::round_rect::AreaType::kMixed;
  }

  const float outer_left_y0 = rect.y0 + rect.ro[kTopLeftCorner];
  const float outer_left_y1 = rect.y1 - rect.ro[kBottomLeftCorner];
  if (y_min >= outer_left_y0 && y_max <= outer_left_y1) {
    if (x_min < rect.x0) return internal::round_rect::AreaType::kMixed;
    const float inner_left_y0 = inner_y0 + inner_radii[kTopLeftCorner];
    const float inner_left_y1 = inner_y1 - inner_radii[kBottomLeftCorner];
    if (y_min < inner_left_y0 || y_max > inner_left_y1) {
      return internal::round_rect::AreaType::kMixed;
    }
    if (x_max <= inner_x0) return internal::round_rect::AreaType::kOutline;
    return internal::round_rect::AreaType::kMixed;
  }

  const float outer_right_y0 = rect.y0 + rect.ro[kTopRightCorner];
  const float outer_right_y1 = rect.y1 - rect.ro[kBottomRightCorner];
  if (y_min >= outer_right_y0 && y_max <= outer_right_y1) {
    if (x_max > rect.x1) return internal::round_rect::AreaType::kMixed;
    const float inner_right_y0 = inner_y0 + inner_radii[kTopRightCorner];
    const float inner_right_y1 = inner_y1 - inner_radii[kBottomRightCorner];
    if (y_min < inner_right_y0 || y_max > inner_right_y1) {
      return internal::round_rect::AreaType::kMixed;
    }
    if (x_min >= inner_x1) return internal::round_rect::AreaType::kOutline;
    return internal::round_rect::AreaType::kMixed;
  }

  return internal::round_rect::AreaType::kMixed;
}

// Called for rectangles with area <= 64 pixels.
internal::round_rect::AreaType DetermineRectColorForRoundRectCornersImpl(
    const SmoothShape::RoundRectCorners& rect, const Box& box) {
  if (BoxInsideRoundRectCornersInteriorHelper(rect, box)) {
    return internal::round_rect::AreaType::kInterior;
  }

  const float x_min = box.xMin() - 0.5f;
  const float y_min = box.yMin() - 0.5f;
  const float x_max = box.xMax() + 0.5f;
  const float y_max = box.yMax() + 0.5f;
  const float inner_x0 = rect.x0 + rect.thickness;
  const float inner_y0 = rect.y0 + rect.thickness;
  const float inner_x1 = rect.x1 - rect.thickness;
  const float inner_y1 = rect.y1 - rect.thickness;
  const float inner_radii[4] = {
      ClampNonNegative(rect.ro[kTopLeftCorner] - rect.thickness),
      ClampNonNegative(rect.ro[kTopRightCorner] - rect.thickness),
      ClampNonNegative(rect.ro[kBottomLeftCorner] - rect.thickness),
      ClampNonNegative(rect.ro[kBottomRightCorner] - rect.thickness),
  };

  const int outer_corner = SelectRoundedCornerBox(
      x_min, y_min, x_max, y_max, rect.x0, rect.y0, rect.x1, rect.y1, rect.ro);
  if (outer_corner >= 0) {
    return DetermineRectColorForRoundRectCornersInCorner(
        rect, x_min, y_min, x_max, y_max, inner_x0, inner_y0, inner_x1,
        inner_y1, inner_radii, outer_corner);
  }

  return DetermineRectColorForRoundRectCornersStraightBand(
      rect, x_min, y_min, x_max, y_max, inner_x0, inner_y0, inner_x1, inner_y1,
      inner_radii);
}

inline float CalcDistSqRectInnerBoundary(const SmoothShape::RoundRect& rect,
                                         float xt, float yt) {
  return CalcDistSqRect(rect.inner_x0, rect.inner_y0, rect.inner_x1,
                        rect.inner_y1, xt, yt);
}

inline bool BoxFullyOutsideRectInnerBoundary(const SmoothShape::RoundRect& rect,
                                             float x_min, float y_min,
                                             float x_max, float y_max) {
  return x_max <= rect.inner_x0 - 0.5f || x_min >= rect.inner_x1 + 0.5f ||
         y_max <= rect.inner_y0 - 0.5f || y_min >= rect.inner_y1 + 0.5f;
}

// Called for rectangles with area <= 64 pixels.
internal::round_rect::AreaType DetermineRectColorForRoundRectImpl(
    const SmoothShape::RoundRect& rect, const Box& box) {
  if (BoxInsideRoundRectInteriorHelper(rect, box)) {
    return internal::round_rect::AreaType::kInterior;
  }
  const bool uses_rect_inner = UsesRectInnerBoundary(rect);
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
  if (!uses_rect_inner && dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq &&
      dbr < r_min_sq) {
    return internal::round_rect::AreaType::kInterior;
  }

  float r_max_sq = rect.ro_sq_adj + rect.ro;
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < rect.x0) {
    if (yMax < rect.y0) {
      if (dbr >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    } else if (yMin > rect.y1) {
      if (dtr >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    }
  } else if (xMin > rect.x1) {
    if (yMax < rect.y0) {
      if (dbl >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    } else if (yMin > rect.y1) {
      if (dtl >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    }
  }
  if (uses_rect_inner) {
    // In rectangular-inner mode, the inner hole can cut through a tile even
    // when the outer circle fully covers it, so prove the tile stays clear of
    // the hole before classifying it as outline.
    if (!BoxFullyOutsideRectInnerBoundary(rect, xMin, yMin, xMax, yMax)) {
      return internal::round_rect::AreaType::kMixed;
    }
    if (xMin >= rect.x0 && xMax <= rect.x1 && yMin >= rect.y0 &&
        yMax <= rect.y1) {
      return internal::round_rect::AreaType::kOutline;
    }

    float outer_full_sq = rect.ro_sq_adj - rect.ro;
    if (xMax <= rect.x1) {
      if (yMax <= rect.y1) {
        if (dtl < outer_full_sq && dbr < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      } else if (yMin >= rect.y0) {
        if (dtr < outer_full_sq && dbl < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      }
    } else if (xMin >= rect.x0) {
      if (yMax <= rect.y1) {
        if (dtr < outer_full_sq && dbl < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      } else if (yMin >= rect.y0) {
        if (dtl < outer_full_sq && dbr < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      }
    }
    return internal::round_rect::AreaType::kMixed;
  }
  // If all corners are in the same quadrant, and all corners are within the
  // ring, then the rect is also within the ring.
  // Once the tile is confined to one corner quadrant, the annulus is convex in
  // that slice, so checking the two diagonal corners is enough to prove the
  // whole tile stays inside the outline band.
  if (xMax <= rect.x1) {
    if (yMax <= rect.y1) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtl < r_ring_max_sq && dtl > r_ring_min_sq && dbr < r_ring_max_sq &&
          dbr > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    } else if (yMin >= rect.y0) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtr < r_ring_max_sq && dtr > r_ring_min_sq && dbl < r_ring_max_sq &&
          dbl > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    }
  } else if (xMin >= rect.x0) {
    if (yMax <= rect.y1) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtr < r_ring_max_sq && dtr > r_ring_min_sq && dbl < r_ring_max_sq &&
          dbl > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    } else if (yMin >= rect.y0) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtl < r_ring_max_sq && dtl > r_ring_min_sq && dbr < r_ring_max_sq &&
          dbr > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    }
  }
  // Slow case; evaluate every pixel from the rectangle.
  return internal::round_rect::AreaType::kMixed;
}

inline int16_t ToDoubledCoord(float value) {
  long doubled = lroundf(2 * value);
  DCHECK_GE(doubled, -32768);
  DCHECK_LE(doubled, 32767);
  return (int16_t)doubled;
}

inline uint32_t Square(int32_t value) {
  return (uint32_t)value * (uint32_t)value;
}

inline uint32_t RowDySq4(int16_t y, int32_t y0x2, int32_t y1x2) {
  int32_t yx2 = 2 * y;
  int32_t ref_yx2 = std::min<int32_t>(std::max<int32_t>(yx2, y0x2), y1x2);
  uint32_t dyx2 = (uint32_t)std::abs(yx2 - ref_yx2);
  return dyx2 * dyx2;
}

inline uint32_t CenterDySq4(int16_t y, int32_t cyx2) {
  uint32_t dyx2 = (uint32_t)std::abs(2 * y - cyx2);
  return dyx2 * dyx2;
}

inline int32_t FloorSqrt(uint32_t value) {
  // `sqrtf` gives a fast initial estimate, but `value` can be larger than the
  // range of integers represented exactly by float. The fixup loops restore
  // the exact integer floor; in practice they are usually no-ops.
  int32_t root = (int32_t)sqrtf((float)value);
  while ((uint64_t)(root + 1) * (uint64_t)(root + 1) <= value) {
    ++root;
  }
  while ((uint64_t)root * (uint64_t)root > value) {
    --root;
  }
  return root;
}

inline int32_t CeilSqrt(uint32_t value) {
  // Build the exact ceil from the corrected floor result so Reset() can seed
  // the outside threshold conservatively.
  int32_t root = FloorSqrt(value);
  return (uint64_t)root * (uint64_t)root == value ? root : root + 1;
}

// Tracks both x thresholds for one rounded boundary band: the largest doubled-x
// fully inside the band and the smallest doubled-x fully outside it.
class CircleBandDxTracker {
 public:
  CircleBandDxTracker(uint32_t full_sq4, uint32_t out_sq4, bool has_full = true,
                      uint32_t initial_dy_sq4 = 0)
      : full_sq4_(full_sq4),
        out_sq4_(out_sq4),
        has_full_(has_full),
        full_dx_max_(-1),
        out_dx_min_(0),
        last_dy_sq4_(0),
        full_error_(0),
        out_error_(0) {
    Reset(initial_dy_sq4);
  }

  // Seeds the tracker directly for one scanline distance. The full threshold
  // starts from a known-inside x bound, and the outside threshold starts from
  // a known-outside x bound; Update() can then tighten them incrementally.
  void Reset(uint32_t dy_sq4) {
    last_dy_sq4_ = dy_sq4;
    if (has_full_) {
      if (dy_sq4 > full_sq4_) {
        full_dx_max_ = -1;
        full_error_ = (int32_t)dy_sq4 - (int32_t)full_sq4_;
      } else {
        full_dx_max_ = FloorSqrt(full_sq4_ - dy_sq4);
        full_error_ =
            (int32_t)(Square(full_dx_max_) + dy_sq4) - (int32_t)full_sq4_;
      }
    } else {
      full_dx_max_ = -1;
      full_error_ = 0;
    }
    if (dy_sq4 >= out_sq4_) {
      out_dx_min_ = 0;
      out_error_ = (int32_t)dy_sq4 - (int32_t)out_sq4_;
    } else {
      out_dx_min_ = CeilSqrt(out_sq4_ - dy_sq4);
      out_error_ = (int32_t)(Square(out_dx_min_) + dy_sq4) - (int32_t)out_sq4_;
    }
  }

  void Update(uint32_t dy_sq4) {
    // Adjacent rows only change dy, so keep the previous error term and nudge
    // the x thresholds just far enough to reestablish the invariants.
    const int32_t delta_dy_sq4 = dy_sq4 >= last_dy_sq4_
                                     ? (int32_t)(dy_sq4 - last_dy_sq4_)
                                     : -(int32_t)(last_dy_sq4_ - dy_sq4);
    last_dy_sq4_ = dy_sq4;
    if (has_full_) {
      UpdateFullDxMax(dy_sq4, delta_dy_sq4);
    } else {
      full_dx_max_ = -1;
    }
    UpdateOutDxMin(dy_sq4, delta_dy_sq4);
  }

  bool has_full() const { return has_full_; }

  uint32_t full_sq4() const { return full_sq4_; }

  uint32_t out_sq4() const { return out_sq4_; }

  int32_t full_dx_max() const { return full_dx_max_; }

  int32_t out_dx_min() const { return out_dx_min_; }

 private:
  void UpdateFullDxMax(uint32_t dy_sq4, int32_t delta_dy_sq4) {
    if (dy_sq4 > full_sq4_) {
      full_dx_max_ = -1;
      full_error_ = (int32_t)dy_sq4 - (int32_t)full_sq4_;
      return;
    }
    if (full_dx_max_ < 0) {
      full_dx_max_ = 0;
      full_error_ = (int32_t)dy_sq4 - (int32_t)full_sq4_;
    } else {
      full_error_ += delta_dy_sq4;
    }
    if (full_error_ > 0) {
      while (full_dx_max_ > 0 && full_error_ > 0) {
        full_error_ -= (int32_t)(2 * full_dx_max_ - 1);
        --full_dx_max_;
      }
    } else {
      while (full_error_ + (int32_t)(2 * full_dx_max_ + 1) <= 0) {
        full_error_ += (int32_t)(2 * full_dx_max_ + 1);
        ++full_dx_max_;
      }
    }
  }

  void UpdateOutDxMin(uint32_t dy_sq4, int32_t delta_dy_sq4) {
    if (dy_sq4 >= out_sq4_) {
      out_dx_min_ = 0;
      out_error_ = (int32_t)dy_sq4 - (int32_t)out_sq4_;
      return;
    }
    if (out_dx_min_ < 0) {
      out_dx_min_ = 0;
      out_error_ = (int32_t)dy_sq4 - (int32_t)out_sq4_;
    } else {
      out_error_ += delta_dy_sq4;
    }
    if (out_error_ < 0) {
      while (out_error_ < 0) {
        out_error_ += (int32_t)(2 * out_dx_min_ + 1);
        ++out_dx_min_;
      }
    } else {
      while (out_dx_min_ > 0 &&
             out_error_ - (int32_t)(2 * out_dx_min_ - 1) >= 0) {
        out_error_ -= (int32_t)(2 * out_dx_min_ - 1);
        --out_dx_min_;
      }
    }
  }

  const uint32_t full_sq4_;
  const uint32_t out_sq4_;
  const bool has_full_;
  int32_t full_dx_max_;
  int32_t out_dx_min_;
  uint32_t last_dy_sq4_;
  int32_t full_error_;
  int32_t out_error_;
};

class RoundRectStream : public PixelStream {
 public:
  using PixelStream::read;

  // Emits a round-rect in row-major order without buffering the whole row.
  // Each scanline is decomposed into a small number of uniform segments:
  // transparent, outline, interior, plus narrow 'slow' edge segments that
  // still need per-pixel anti-aliased evaluation.
  RoundRectStream(const SmoothShape::RoundRect& rect, Box bounds)
      : rect_(rect),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()),
        segment_count_(0),
        segment_index_(0),
        row_ready_(false),
        x0x2_(ToDoubledCoord(rect.x0)),
        y0x2_(ToDoubledCoord(rect.y0)),
        x1x2_(ToDoubledCoord(rect.x1)),
        y1x2_(ToDoubledCoord(rect.y1)),
        rox2_(ToDoubledCoord(rect.ro)),
        rix2_(ToDoubledCoord(rect.ri)),
        outer_tracker_(Square(std::max<int32_t>(0, rox2_ - 1)),
                       Square(rox2_ + 1), true,
                       RowDySq4(bounds_.yMin(), y0x2_, y1x2_)),
        inner_tracker_(rix2_ > 0 ? Square(rix2_ - 1) : 0, Square(rix2_ + 1),
                       rix2_ > 0, RowDySq4(bounds_.yMin(), y0x2_, y1x2_)) {}

  void read(Color* buf, uint16_t count, uint32_t& run_length) override {
    run_length = 0;
    while (count > 0) {
      if (!row_ready_) PrepareRow();
      if (segment_index_ >= segment_count_) return;
      const Segment& segment = segments_[segment_index_];
      uint16_t batch = segment.end_x - x_ + 1;
      if (batch > count) batch = count;
      switch (segment.kind) {
        case SegmentKind::kTransparent:
          FillColor(buf, batch, color::Transparent);
          break;
        case SegmentKind::kInterior:
          FillColor(buf, batch, rect_.interior_color);
          break;
        case SegmentKind::kOutline:
          FillColor(buf, batch, rect_.outline_color);
          break;
        case SegmentKind::kSlow:
          for (uint16_t i = 0; i < batch; ++i) {
            buf[i] = GetSmoothRoundRectPixelColor(rect_, x_ + i, y_);
          }
          break;
      }
      buf += batch;
      count -= batch;
      x_ += batch;
      if (x_ > segment.end_x) {
        ++segment_index_;
      }
      if (x_ > bounds_.xMax()) {
        x_ = bounds_.xMin();
        ++y_;
        row_ready_ = false;
      }
    }
  }

  void skip(uint32_t count) override {
    uint16_t width = bounds_.width();
    y_ += count / width;
    x_ += count % width;
    if (x_ > bounds_.xMax()) {
      x_ -= width;
      ++y_;
    }
    row_ready_ = false;
    segment_count_ = 0;
    segment_index_ = 0;
  }

 private:
  enum class SegmentKind : uint8_t {
    kTransparent,
    kInterior,
    kOutline,
    kSlow,
  };

  struct Segment {
    int16_t start_x;
    int16_t end_x;
    SegmentKind kind;
  };

  inline int16_t FloorDiv2(int32_t value) {
    return value >= 0 ? value / 2 : -(((-value) + 1) / 2);
  }

  inline int16_t CeilDiv2(int32_t value) {
    return value >= 0 ? (value + 1) / 2 : -((-value) / 2);
  }

  void UpdateThresholds(uint32_t dy_sq4) {
    outer_tracker_.Update(dy_sq4);
    inner_tracker_.Update(dy_sq4);
  }

  void AddSegment(int16_t start_x, int16_t end_x, SegmentKind kind) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == kind && prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] = Segment{start_x, end_x, kind};
  }

  SegmentKind ClassifyZeroDx(uint32_t dy_sq4) const {
    // The center slab has dx = 0, so this row segment only depends on how far
    // the scanline sits from the vertical span of the rounded rectangle.
    if (inner_tracker_.has_full() && dy_sq4 <= inner_tracker_.full_sq4()) {
      return SegmentKind::kInterior;
    }
    if (dy_sq4 >= outer_tracker_.out_sq4()) {
      return SegmentKind::kTransparent;
    }
    if (dy_sq4 <= outer_tracker_.full_sq4() &&
        (rox2_ == rix2_ || dy_sq4 >= inner_tracker_.out_sq4())) {
      return SegmentKind::kOutline;
    }
    return SegmentKind::kSlow;
  }

  void PrepareLeftSide(int16_t start_x, int16_t end_x, int32_t trans_dx_min,
                       int32_t outer_dx_max, int32_t inner_out_dx_min,
                       int32_t inner_dx_max) {
    if (start_x > end_x) return;
    // Each rounded cap breaks into at most five monotonic regions:
    // transparent, slow edge, outline, slow edge, interior.
    int16_t transparent_end = FloorDiv2(x0x2_ - trans_dx_min);
    int16_t outer_start = CeilDiv2(x0x2_ - outer_dx_max);
    int16_t outline_end = FloorDiv2(x0x2_ - inner_out_dx_min);
    int16_t interior_start = CeilDiv2(x0x2_ - inner_dx_max);

    AddSegment(start_x, std::min(end_x, transparent_end),
               SegmentKind::kTransparent);
    AddSegment(std::max(start_x, (int16_t)(transparent_end + 1)),
               std::min(end_x, (int16_t)(outer_start - 1)), SegmentKind::kSlow);
    AddSegment(std::max(start_x, outer_start), std::min(end_x, outline_end),
               SegmentKind::kOutline);
    AddSegment(std::max(start_x, (int16_t)(outline_end + 1)),
               std::min(end_x, (int16_t)(interior_start - 1)),
               SegmentKind::kSlow);
    AddSegment(std::max(start_x, interior_start), end_x,
               SegmentKind::kInterior);
  }

  void PrepareRightSide(int16_t start_x, int16_t end_x, int32_t trans_dx_min,
                        int32_t outer_dx_max, int32_t inner_out_dx_min,
                        int32_t inner_dx_max) {
    if (start_x > end_x) return;
    int16_t interior_end = FloorDiv2(x1x2_ + inner_dx_max);
    int16_t outline_start = CeilDiv2(x1x2_ + inner_out_dx_min);
    int16_t outer_end = FloorDiv2(x1x2_ + outer_dx_max);
    int16_t transparent_start = CeilDiv2(x1x2_ + trans_dx_min);

    AddSegment(start_x, std::min(end_x, interior_end), SegmentKind::kInterior);
    AddSegment(std::max(start_x, (int16_t)(interior_end + 1)),
               std::min(end_x, (int16_t)(outline_start - 1)),
               SegmentKind::kSlow);
    AddSegment(std::max(start_x, outline_start), std::min(end_x, outer_end),
               SegmentKind::kOutline);
    AddSegment(std::max(start_x, (int16_t)(outer_end + 1)),
               std::min(end_x, (int16_t)(transparent_start - 1)),
               SegmentKind::kSlow);
    AddSegment(std::max(start_x, transparent_start), end_x,
               SegmentKind::kTransparent);
  }

  void PrepareRow() {
    segment_count_ = 0;
    segment_index_ = 0;
    row_ready_ = true;
    if (y_ > bounds_.yMax()) return;

    // Distances are tracked in doubled coordinates so the +/-0.5 pixel margins
    // used by the anti-aliased round-rect tests become exact integer
    // thresholds. For the current row we reduce the shape to three zones:
    // left cap, center slab, right cap.
    uint32_t dy_sq4 = RowDySq4(y_, y0x2_, y1x2_);

    UpdateThresholds(dy_sq4);

    int16_t zero_start = CeilDiv2(x0x2_);
    int16_t zero_end = FloorDiv2(x1x2_);

    PrepareLeftSide(bounds_.xMin(),
                    std::min(bounds_.xMax(), (int16_t)(zero_start - 1)),
                    outer_tracker_.out_dx_min(), outer_tracker_.full_dx_max(),
                    inner_tracker_.out_dx_min(), inner_tracker_.full_dx_max());

    int16_t center_start = std::max(bounds_.xMin(), zero_start);
    int16_t center_end = std::min(bounds_.xMax(), zero_end);
    if (center_start <= center_end) {
      AddSegment(center_start, center_end, ClassifyZeroDx(dy_sq4));
    }

    PrepareRightSide(std::max(bounds_.xMin(), (int16_t)(zero_end + 1)),
                     bounds_.xMax(), outer_tracker_.out_dx_min(),
                     outer_tracker_.full_dx_max(), inner_tracker_.out_dx_min(),
                     inner_tracker_.full_dx_max());
  }

  const SmoothShape::RoundRect& rect_;
  Box bounds_;
  int16_t x_;
  int16_t y_;
  // Worst case is five left-cap spans, one center span, and five right-cap
  // spans before adjacent equal-color spans are merged.
  Segment segments_[11];
  uint8_t segment_count_;
  uint8_t segment_index_;
  bool row_ready_;

  // Stored in doubled coordinates so half-pixel AA thresholds stay integral.
  // Under the practical Box coordinate bounds, the doubled values fit in
  // int16_t even though combined expressions still need wider temporaries.
  const int16_t x0x2_;
  const int16_t y0x2_;
  const int16_t x1x2_;
  const int16_t y1x2_;
  const int16_t rox2_;
  const int16_t rix2_;

  // Current scanline state for the incremental circle trackers.
  CircleBandDxTracker outer_tracker_;
  CircleBandDxTracker inner_tracker_;
};

// Rectangular-inner round rects share the same rounded outer boundary but keep
// the inner geometry axis-aligned, so the stream can recover long uniform runs
// without touching the rounded-inner hot path.
class RectInnerRoundRectStream : public PixelStream {
 public:
  using PixelStream::read;

  RectInnerRoundRectStream(const SmoothShape::RoundRect& rect, Box bounds)
      : rect_(rect),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()),
        segment_count_(0),
        segment_index_(0),
        row_ready_(false),
        x0x2_(ToDoubledCoord(rect.x0)),
        y0x2_(ToDoubledCoord(rect.y0)),
        x1x2_(ToDoubledCoord(rect.x1)),
        y1x2_(ToDoubledCoord(rect.y1)),
        rox2_(ToDoubledCoord(rect.ro)),
        outer_tracker_(Square(std::max<int32_t>(0, rox2_ - 1)),
                       Square(rox2_ + 1), true,
                       RowDySq4(bounds_.yMin(), y0x2_, y1x2_)),
        inner_inside_start_((int16_t)ceilf(rect.inner_x0)),
        inner_inside_end_((int16_t)floorf(rect.inner_x1)),
        inner_outside_left_end_((int16_t)floorf(rect.inner_x0 - 0.5f)),
        inner_outside_right_start_((int16_t)ceilf(rect.inner_x1 + 0.5f)) {}

  void read(Color* buf, uint16_t count, uint32_t& run_length) override {
    run_length = 0;
    while (count > 0) {
      if (!row_ready_) PrepareRow();
      if (segment_index_ >= segment_count_) return;
      const Segment& segment = segments_[segment_index_];
      uint16_t batch = segment.end_x - x_ + 1;
      if (batch > count) batch = count;
      switch (segment.kind) {
        case SegmentKind::kSolid:
          FillColor(buf, batch, segment.color);
          break;
        case SegmentKind::kSlow:
          for (uint16_t i = 0; i < batch; ++i) {
            buf[i] = GetSmoothRoundRectPixelColor(rect_, x_ + i, y_);
          }
          break;
      }
      buf += batch;
      count -= batch;
      x_ += batch;
      if (x_ > segment.end_x) {
        ++segment_index_;
      }
      if (x_ > bounds_.xMax()) {
        x_ = bounds_.xMin();
        ++y_;
        row_ready_ = false;
      }
    }
  }

  void skip(uint32_t count) override {
    uint16_t width = bounds_.width();
    y_ += count / width;
    x_ += count % width;
    if (x_ > bounds_.xMax()) {
      x_ -= width;
      ++y_;
    }
    row_ready_ = false;
    segment_count_ = 0;
    segment_index_ = 0;
  }

 private:
  enum class SegmentKind : uint8_t {
    kSolid,
    kSlow,
  };

  struct Segment {
    int16_t start_x;
    int16_t end_x;
    SegmentKind kind;
    Color color;
  };

  inline int16_t FloorDiv2(int32_t value) {
    return value >= 0 ? value / 2 : -(((-value) + 1) / 2);
  }

  inline int16_t CeilDiv2(int32_t value) {
    return value >= 0 ? (value + 1) / 2 : -((-value) / 2);
  }

  void AddSolidSegment(int16_t start_x, int16_t end_x, Color color) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == SegmentKind::kSolid && prev.color == color &&
          prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] =
        Segment{start_x, end_x, SegmentKind::kSolid, color};
  }

  void AddSlowSegment(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == SegmentKind::kSlow && prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] =
        Segment{start_x, end_x, SegmentKind::kSlow, color::Transparent};
  }

  void AddSampledSolidSegment(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;
    AddSolidSegment(start_x, end_x,
                    GetSmoothRoundRectPixelColor(rect_, start_x, y_));
  }

  void AddFullOuterSegments(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;

    // Once the rounded outer boundary fully covers this span, only the
    // axis-aligned inner rectangle can introduce color changes.
    const float row_y = y_;
    if (row_y <= rect_.inner_y0 - 0.5f || row_y >= rect_.inner_y1 + 0.5f) {
      AddSolidSegment(start_x, end_x, rect_.outline_color);
      return;
    }

    AddSolidSegment(start_x, std::min(end_x, inner_outside_left_end_),
                    rect_.outline_color);
    AddSlowSegment(std::max(start_x, (int16_t)(inner_outside_left_end_ + 1)),
                   std::min(end_x, (int16_t)(inner_inside_start_ - 1)));

    const int16_t inside_start = std::max(start_x, inner_inside_start_);
    const int16_t inside_end = std::min(end_x, inner_inside_end_);
    if (inside_start <= inside_end) {
      const bool has_full_interior = !rect_.inner_mid.empty() &&
                                     y_ >= rect_.inner_mid.yMin() &&
                                     y_ <= rect_.inner_mid.yMax();
      if (has_full_interior) {
        AddSampledSolidSegment(
            inside_start,
            std::min(inside_end, (int16_t)(rect_.inner_mid.xMin() - 1)));
        AddSolidSegment(std::max(inside_start, rect_.inner_mid.xMin()),
                        std::min(inside_end, rect_.inner_mid.xMax()),
                        rect_.interior_color);
        AddSampledSolidSegment(
            std::max(inside_start, (int16_t)(rect_.inner_mid.xMax() + 1)),
            inside_end);
      } else {
        AddSampledSolidSegment(inside_start, inside_end);
      }
    }

    AddSlowSegment(std::max(start_x, (int16_t)(inner_inside_end_ + 1)),
                   std::min(end_x, (int16_t)(inner_outside_right_start_ - 1)));
    AddSolidSegment(std::max(start_x, inner_outside_right_start_), end_x,
                    rect_.outline_color);
  }

  void PrepareRow() {
    segment_count_ = 0;
    segment_index_ = 0;
    row_ready_ = true;
    if (y_ > bounds_.yMax()) return;

    // This stream only tracks the rounded outer edge. After that, the inner
    // rectangle turns the fully covered span into a few axis-aligned runs.
    uint32_t dy_sq4 = RowDySq4(y_, y0x2_, y1x2_);

    outer_tracker_.Update(dy_sq4);

    if (outer_tracker_.out_dx_min() == 0) {
      AddSolidSegment(bounds_.xMin(), bounds_.xMax(), color::Transparent);
      return;
    }

    int16_t transparent_end = FloorDiv2(x0x2_ - outer_tracker_.out_dx_min());
    int16_t transparent_start = CeilDiv2(x1x2_ + outer_tracker_.out_dx_min());

    AddSolidSegment(bounds_.xMin(), std::min(bounds_.xMax(), transparent_end),
                    color::Transparent);

    if (outer_tracker_.full_dx_max() < 0) {
      AddSlowSegment(
          std::max(bounds_.xMin(), (int16_t)(transparent_end + 1)),
          std::min(bounds_.xMax(), (int16_t)(transparent_start - 1)));
    } else {
      int16_t outer_full_start = CeilDiv2(x0x2_ - outer_tracker_.full_dx_max());
      int16_t outer_full_end = FloorDiv2(x1x2_ + outer_tracker_.full_dx_max());
      AddSlowSegment(std::max(bounds_.xMin(), (int16_t)(transparent_end + 1)),
                     std::min(bounds_.xMax(), (int16_t)(outer_full_start - 1)));
      AddFullOuterSegments(std::max(bounds_.xMin(), outer_full_start),
                           std::min(bounds_.xMax(), outer_full_end));
      AddSlowSegment(
          std::max(bounds_.xMin(), (int16_t)(outer_full_end + 1)),
          std::min(bounds_.xMax(), (int16_t)(transparent_start - 1)));
    }

    AddSolidSegment(std::max(bounds_.xMin(), transparent_start), bounds_.xMax(),
                    color::Transparent);
  }

  const SmoothShape::RoundRect& rect_;
  Box bounds_;
  int16_t x_;
  int16_t y_;
  Segment segments_[11];
  uint8_t segment_count_;
  uint8_t segment_index_;
  bool row_ready_;

  const int16_t x0x2_;
  const int16_t y0x2_;
  const int16_t x1x2_;
  const int16_t y1x2_;
  const int16_t rox2_;

  CircleBandDxTracker outer_tracker_;

  const int16_t inner_inside_start_;
  const int16_t inner_inside_end_;
  const int16_t inner_outside_left_end_;
  const int16_t inner_outside_right_start_;
};

// Unequal-radius round rects still stream row-by-row, but each side can now
// switch independently between a top corner, a straight edge, and a bottom
// corner. The stream keeps one tracker per corner and only falls back to the
// pixel evaluator for rows or spans that remain in an AA fringe.
class RoundRectCornersStream : public PixelStream {
 public:
  using PixelStream::read;

  RoundRectCornersStream(const SmoothShape::RoundRectCorners& rect, Box bounds)
      : rect_(rect),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()),
        segment_count_(0),
        segment_index_(0),
        row_ready_(false),
        inner_x0_(rect.x0 + rect.thickness),
        inner_y0_(rect.y0 + rect.thickness),
        inner_x1_(rect.x1 - rect.thickness),
        inner_y1_(rect.y1 - rect.thickness),
        inner_radii_{
            ClampNonNegative(rect.ro[kTopLeftCorner] - rect.thickness),
            ClampNonNegative(rect.ro[kTopRightCorner] - rect.thickness),
            ClampNonNegative(rect.ro[kBottomLeftCorner] - rect.thickness),
            ClampNonNegative(rect.ro[kBottomRightCorner] - rect.thickness)},
        outer_trackers_{
            CornerTracker(rect.x0 + rect.ro[kTopLeftCorner],
                          rect.y0 + rect.ro[kTopLeftCorner],
                          rect.ro[kTopLeftCorner], bounds_.yMin()),
            CornerTracker(rect.x1 - rect.ro[kTopRightCorner],
                          rect.y0 + rect.ro[kTopRightCorner],
                          rect.ro[kTopRightCorner], bounds_.yMin()),
            CornerTracker(rect.x0 + rect.ro[kBottomLeftCorner],
                          rect.y1 - rect.ro[kBottomLeftCorner],
                          rect.ro[kBottomLeftCorner], bounds_.yMin()),
            CornerTracker(rect.x1 - rect.ro[kBottomRightCorner],
                          rect.y1 - rect.ro[kBottomRightCorner],
                          rect.ro[kBottomRightCorner], bounds_.yMin())},
        inner_trackers_{
            CornerTracker(inner_x0_ + inner_radii_[kTopLeftCorner],
                          inner_y0_ + inner_radii_[kTopLeftCorner],
                          inner_radii_[kTopLeftCorner], bounds_.yMin()),
            CornerTracker(inner_x1_ - inner_radii_[kTopRightCorner],
                          inner_y0_ + inner_radii_[kTopRightCorner],
                          inner_radii_[kTopRightCorner], bounds_.yMin()),
            CornerTracker(inner_x0_ + inner_radii_[kBottomLeftCorner],
                          inner_y1_ - inner_radii_[kBottomLeftCorner],
                          inner_radii_[kBottomLeftCorner], bounds_.yMin()),
            CornerTracker(inner_x1_ - inner_radii_[kBottomRightCorner],
                          inner_y1_ - inner_radii_[kBottomRightCorner],
                          inner_radii_[kBottomRightCorner], bounds_.yMin())},
        outer_inside_top_((int16_t)ceilf(rect.y0 + 0.5f)),
        outer_inside_bottom_((int16_t)floorf(rect.y1 - 0.5f)),
        outer_outside_left_end_((int16_t)floorf(rect.x0 - 0.5f)),
        outer_inside_left_start_((int16_t)ceilf(rect.x0 + 0.5f)),
        outer_inside_right_end_((int16_t)floorf(rect.x1 - 0.5f)),
        outer_outside_right_start_((int16_t)ceilf(rect.x1 + 0.5f)),
        inner_outside_top_end_((int16_t)floorf(inner_y0_ - 0.5f)),
        inner_inside_top_start_((int16_t)ceilf(inner_y0_ + 0.5f)),
        inner_inside_bottom_end_((int16_t)floorf(inner_y1_ - 0.5f)),
        inner_outside_bottom_start_((int16_t)ceilf(inner_y1_ + 0.5f)),
        inner_outside_left_end_((int16_t)floorf(inner_x0_ - 0.5f)),
        inner_inside_left_start_((int16_t)ceilf(inner_x0_ + 0.5f)),
        inner_inside_right_end_((int16_t)floorf(inner_x1_ - 0.5f)),
        inner_outside_right_start_((int16_t)ceilf(inner_x1_ + 0.5f)) {}

  void read(Color* buf, uint16_t count, uint32_t& run_length) override {
    run_length = 0;
    while (count > 0) {
      if (!row_ready_) PrepareRow();
      if (segment_index_ >= segment_count_) return;
      const Segment& segment = segments_[segment_index_];
      uint16_t batch = segment.end_x - x_ + 1;
      if (batch > count) batch = count;
      switch (segment.kind) {
        case SegmentKind::kSolid:
          FillColor(buf, batch, segment.color);
          break;
        case SegmentKind::kSlow:
          for (uint16_t i = 0; i < batch; ++i) {
            buf[i] = GetSmoothRoundRectCornersPixelColor(rect_, x_ + i, y_);
          }
          break;
      }
      buf += batch;
      count -= batch;
      x_ += batch;
      if (x_ > segment.end_x) {
        ++segment_index_;
      }
      if (x_ > bounds_.xMax()) {
        x_ = bounds_.xMin();
        ++y_;
        row_ready_ = false;
      }
    }
  }

  void skip(uint32_t count) override {
    uint16_t width = bounds_.width();
    y_ += count / width;
    x_ += count % width;
    if (x_ > bounds_.xMax()) {
      x_ -= width;
      ++y_;
    }
    row_ready_ = false;
    segment_count_ = 0;
    segment_index_ = 0;
  }

 private:
  struct CornerTracker {
    CornerTracker(float center_x, float center_y, float radius, int16_t y)
        : center_x(center_x),
          center_y(center_y),
          full_sq(radius * radius - radius + 0.25f),
          out_sq(radius * radius + radius + 0.25f),
          has_full(radius >= 0.5f),
          full_dx(-1.0f),
          out_dx(0.0f) {
      Update(y);
    }

    void Update(int16_t y) {
      const float dy = y - center_y;
      const float dy_sq = dy * dy;
      if (has_full && dy_sq <= full_sq) {
        full_dx = sqrtf(full_sq - dy_sq);
      } else {
        full_dx = -1.0f;
      }
      if (dy_sq < out_sq) {
        out_dx = sqrtf(out_sq - dy_sq);
      } else {
        out_dx = 0.0f;
      }
    }

    float center_x;
    float center_y;
    float full_sq;
    float out_sq;
    bool has_full;
    float full_dx;
    float out_dx;
  };

  enum class SegmentKind : uint8_t {
    kSolid,
    kSlow,
  };

  struct Segment {
    int16_t start_x;
    int16_t end_x;
    SegmentKind kind;
    Color color;
  };

  inline int16_t FloorDiv2(int32_t value) {
    return value >= 0 ? value / 2 : -(((-value) + 1) / 2);
  }

  inline int16_t CeilDiv2(int32_t value) {
    return value >= 0 ? (value + 1) / 2 : -((-value) / 2);
  }

  void AddSolidSegment(int16_t start_x, int16_t end_x, Color color) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == SegmentKind::kSolid && prev.color == color &&
          prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] =
        Segment{start_x, end_x, SegmentKind::kSolid, color};
  }

  void AddSlowSegment(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == SegmentKind::kSlow && prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] =
        Segment{start_x, end_x, SegmentKind::kSlow, color::Transparent};
  }

  void UpdateTrackers() {
    for (CornerTracker& tracker : outer_trackers_) {
      tracker.Update(y_);
    }
    for (CornerTracker& tracker : inner_trackers_) {
      tracker.Update(y_);
    }
  }

  bool RowHasFullOuterCoverage() const {
    return y_ >= outer_inside_top_ && y_ <= outer_inside_bottom_;
  }

  bool RowFullyOutsideInner() const {
    return y_ <= inner_outside_top_end_ || y_ >= inner_outside_bottom_start_;
  }

  bool RowHasFullInteriorCoverage() const {
    return y_ >= inner_inside_top_start_ && y_ <= inner_inside_bottom_end_;
  }

  void LeftOuterThresholds(int16_t* transparent_end, int16_t* full_start) {
    if (rect_.ro[kTopLeftCorner] > 0.0f &&
        y_ < rect_.y0 + rect_.ro[kTopLeftCorner]) {
      const CornerTracker& corner = outer_trackers_[kTopLeftCorner];
      *transparent_end = (int16_t)floorf(corner.center_x - corner.out_dx);
      *full_start = corner.full_dx >= 0.0f
                        ? (int16_t)ceilf(corner.center_x - corner.full_dx)
                        : (int16_t)ceilf(corner.center_x + 1.0f);
      return;
    }
    if (rect_.ro[kBottomLeftCorner] > 0.0f &&
        y_ > rect_.y1 - rect_.ro[kBottomLeftCorner]) {
      const CornerTracker& corner = outer_trackers_[kBottomLeftCorner];
      *transparent_end = (int16_t)floorf(corner.center_x - corner.out_dx);
      *full_start = corner.full_dx >= 0.0f
                        ? (int16_t)ceilf(corner.center_x - corner.full_dx)
                        : (int16_t)ceilf(corner.center_x + 1.0f);
      return;
    }
    *transparent_end = outer_outside_left_end_;
    *full_start = outer_inside_left_start_;
  }

  void RightOuterThresholds(int16_t* full_end, int16_t* transparent_start) {
    if (rect_.ro[kTopRightCorner] > 0.0f &&
        y_ < rect_.y0 + rect_.ro[kTopRightCorner]) {
      const CornerTracker& corner = outer_trackers_[kTopRightCorner];
      *full_end = corner.full_dx >= 0.0f
                      ? (int16_t)floorf(corner.center_x + corner.full_dx)
                      : (int16_t)floorf(corner.center_x - 1.0f);
      *transparent_start = (int16_t)ceilf(corner.center_x + corner.out_dx);
      return;
    }
    if (rect_.ro[kBottomRightCorner] > 0.0f &&
        y_ > rect_.y1 - rect_.ro[kBottomRightCorner]) {
      const CornerTracker& corner = outer_trackers_[kBottomRightCorner];
      *full_end = corner.full_dx >= 0.0f
                      ? (int16_t)floorf(corner.center_x + corner.full_dx)
                      : (int16_t)floorf(corner.center_x - 1.0f);
      *transparent_start = (int16_t)ceilf(corner.center_x + corner.out_dx);
      return;
    }
    *full_end = outer_inside_right_end_;
    *transparent_start = outer_outside_right_start_;
  }

  void LeftInnerThresholds(int16_t* outside_end, int16_t* inside_start) {
    if (inner_radii_[kTopLeftCorner] > 0.0f &&
        y_ < inner_y0_ + inner_radii_[kTopLeftCorner]) {
      const CornerTracker& corner = inner_trackers_[kTopLeftCorner];
      *outside_end = (int16_t)floorf(corner.center_x - corner.out_dx);
      *inside_start = corner.full_dx >= 0.0f
                          ? (int16_t)ceilf(corner.center_x - corner.full_dx)
                          : (int16_t)ceilf(corner.center_x + 1.0f);
      return;
    }
    if (inner_radii_[kBottomLeftCorner] > 0.0f &&
        y_ > inner_y1_ - inner_radii_[kBottomLeftCorner]) {
      const CornerTracker& corner = inner_trackers_[kBottomLeftCorner];
      *outside_end = (int16_t)floorf(corner.center_x - corner.out_dx);
      *inside_start = corner.full_dx >= 0.0f
                          ? (int16_t)ceilf(corner.center_x - corner.full_dx)
                          : (int16_t)ceilf(corner.center_x + 1.0f);
      return;
    }
    *outside_end = inner_outside_left_end_;
    *inside_start = inner_inside_left_start_;
  }

  void RightInnerThresholds(int16_t* inside_end, int16_t* outside_start) {
    if (inner_radii_[kTopRightCorner] > 0.0f &&
        y_ < inner_y0_ + inner_radii_[kTopRightCorner]) {
      const CornerTracker& corner = inner_trackers_[kTopRightCorner];
      *inside_end = corner.full_dx >= 0.0f
                        ? (int16_t)floorf(corner.center_x + corner.full_dx)
                        : (int16_t)floorf(corner.center_x - 1.0f);
      *outside_start = (int16_t)ceilf(corner.center_x + corner.out_dx);
      return;
    }
    if (inner_radii_[kBottomRightCorner] > 0.0f &&
        y_ > inner_y1_ - inner_radii_[kBottomRightCorner]) {
      const CornerTracker& corner = inner_trackers_[kBottomRightCorner];
      *inside_end = corner.full_dx >= 0.0f
                        ? (int16_t)floorf(corner.center_x + corner.full_dx)
                        : (int16_t)floorf(corner.center_x - 1.0f);
      *outside_start = (int16_t)ceilf(corner.center_x + corner.out_dx);
      return;
    }
    *inside_end = inner_inside_right_end_;
    *outside_start = inner_outside_right_start_;
  }

  void AddFullOuterSegments(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;
    if (RowFullyOutsideInner()) {
      AddSolidSegment(start_x, end_x, rect_.outline_color);
      return;
    }
    if (!RowHasFullInteriorCoverage()) {
      AddSlowSegment(start_x, end_x);
      return;
    }

    int16_t outline_left_end;
    int16_t interior_start;
    int16_t interior_end;
    int16_t outline_right_start;
    LeftInnerThresholds(&outline_left_end, &interior_start);
    RightInnerThresholds(&interior_end, &outline_right_start);

    AddSolidSegment(start_x, std::min(end_x, outline_left_end),
                    rect_.outline_color);
    AddSlowSegment(std::max(start_x, (int16_t)(outline_left_end + 1)),
                   std::min(end_x, (int16_t)(interior_start - 1)));
    AddSolidSegment(std::max(start_x, interior_start),
                    std::min(end_x, interior_end), rect_.interior_color);
    AddSlowSegment(std::max(start_x, (int16_t)(interior_end + 1)),
                   std::min(end_x, (int16_t)(outline_right_start - 1)));
    AddSolidSegment(std::max(start_x, outline_right_start), end_x,
                    rect_.outline_color);
  }

  void PrepareRow() {
    segment_count_ = 0;
    segment_index_ = 0;
    row_ready_ = true;
    if (y_ > bounds_.yMax()) return;

    UpdateTrackers();

    int16_t transparent_end;
    int16_t outer_full_start;
    int16_t outer_full_end;
    int16_t transparent_start;
    LeftOuterThresholds(&transparent_end, &outer_full_start);
    RightOuterThresholds(&outer_full_end, &transparent_start);

    AddSolidSegment(bounds_.xMin(), std::min(bounds_.xMax(), transparent_end),
                    color::Transparent);

    const int16_t slow_start =
        std::max(bounds_.xMin(), (int16_t)(transparent_end + 1));
    const int16_t slow_end =
        std::min(bounds_.xMax(), (int16_t)(transparent_start - 1));
    if (!RowHasFullOuterCoverage() || outer_full_start > outer_full_end) {
      AddSlowSegment(slow_start, slow_end);
    } else {
      AddSlowSegment(slow_start,
                     std::min(bounds_.xMax(), (int16_t)(outer_full_start - 1)));
      AddFullOuterSegments(std::max(bounds_.xMin(), outer_full_start),
                           std::min(bounds_.xMax(), outer_full_end));
      AddSlowSegment(std::max(bounds_.xMin(), (int16_t)(outer_full_end + 1)),
                     slow_end);
    }

    AddSolidSegment(std::max(bounds_.xMin(), transparent_start), bounds_.xMax(),
                    color::Transparent);
  }

  const SmoothShape::RoundRectCorners& rect_;
  Box bounds_;
  int16_t x_;
  int16_t y_;
  Segment segments_[9];
  uint8_t segment_count_;
  uint8_t segment_index_;
  bool row_ready_;

  const float inner_x0_;
  const float inner_y0_;
  const float inner_x1_;
  const float inner_y1_;
  const float inner_radii_[4];
  std::array<CornerTracker, 4> outer_trackers_;
  std::array<CornerTracker, 4> inner_trackers_;

  const int16_t outer_inside_top_;
  const int16_t outer_inside_bottom_;
  const int16_t outer_outside_left_end_;
  const int16_t outer_inside_left_start_;
  const int16_t outer_inside_right_end_;
  const int16_t outer_outside_right_start_;

  const int16_t inner_outside_top_end_;
  const int16_t inner_inside_top_start_;
  const int16_t inner_inside_bottom_end_;
  const int16_t inner_outside_bottom_start_;
  const int16_t inner_outside_left_end_;
  const int16_t inner_inside_left_start_;
  const int16_t inner_inside_right_end_;
  const int16_t inner_outside_right_start_;
};

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
  // Small tiles still go through the same classifier first so uniform regions
  // collapse to a single fillRect and only genuinely mixed tiles sample pixels.
  switch (DetermineRectColorForRoundRectImpl(rect, box)) {
    case internal::round_rect::AreaType::kExterior: {
      if (spec.fill_mode == FillMode::kExtents) {
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case internal::round_rect::AreaType::kInterior: {
      if (spec.fill_mode == FillMode::kExtents ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case internal::round_rect::AreaType::kOutline: {
      if (spec.fill_mode == FillMode::kExtents ||
          outline != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_outline);
        return;
      }
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FillMode::kVisible) {
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
  // Tile into 8x8 blocks so the rectangle classifier gets repeated chances to
  // prove uniform fill regions without allocating a full temporary buffer.
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

// Called for rectangles with area <= 64 pixels.
inline void FillSubrectOfRoundRectCorners(
    const SmoothShape::RoundRectCorners& rect, const RoundRectDrawSpec& spec,
    const Box& box) {
  Color interior = rect.interior_color;
  Color outline = rect.outline_color;
  switch (DetermineRectColorForRoundRectCornersImpl(rect, box)) {
    case internal::round_rect::AreaType::kExterior: {
      if (spec.fill_mode == FillMode::kExtents) {
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case internal::round_rect::AreaType::kInterior: {
      if (spec.fill_mode == FillMode::kExtents ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case internal::round_rect::AreaType::kOutline: {
      if (spec.fill_mode == FillMode::kExtents ||
          outline != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_outline);
      }
      return;
    }
    default:
      break;
  }

  if (spec.fill_mode == FillMode::kVisible) {
    BufferedPixelWriter writer(*spec.out, spec.blending_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = PointInsideRoundRectCornersInteriorHelper(rect, x, y)
                      ? interior
                      : GetSmoothRoundRectCornersPixelColor(rect, x, y);
        if (c == color::Transparent) continue;
        writer.writePixel(x, y,
                          c == interior  ? spec.pre_blended_interior
                          : c == outline ? spec.pre_blended_outline
                                         : AlphaBlend(spec.bgcolor, c));
      }
    }
    return;
  }

  Color color[64];
  int cnt = 0;
  for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
    for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
      Color c = PointInsideRoundRectCornersInteriorHelper(rect, x, y)
                    ? interior
                    : GetSmoothRoundRectCornersPixelColor(rect, x, y);
      color[cnt++] = c.a() == 0      ? spec.bgcolor
                     : c == interior ? spec.pre_blended_interior
                     : c == outline  ? spec.pre_blended_outline
                                     : AlphaBlend(spec.bgcolor, c);
    }
  }
  spec.out->setAddress(box, spec.blending_mode);
  spec.out->write(color, cnt);
}

void FillSubrectangle(const SmoothShape::RoundRectCorners& rect,
                      const RoundRectDrawSpec& spec, const Box& box) {
  const int16_t xMinOuter = (box.xMin() / 8) * 8;
  const int16_t yMinOuter = (box.yMin() / 8) * 8;
  const int16_t xMaxOuter = (box.xMax() / 8) * 8 + 7;
  const int16_t yMaxOuter = (box.yMax() / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSubrectOfRoundRectCorners(
          rect, spec,
          Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
              std::min((int16_t)(x + 7), box.xMax()),
              std::min((int16_t)(y + 7), box.yMax())));
    }
  }
}

void DrawRoundRectImpl(SmoothShape::RoundRect rect, const Surface& s,
                       const Box& box) {
  RoundRectDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .blending_mode = s.blending_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_outline = AlphaBlend(s.bgcolor(), rect.outline_color),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), rect.interior_color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    rect.x0 += s.dx();
    rect.y0 += s.dy();
    rect.x1 += s.dx();
    rect.y1 += s.dy();
    rect.inner_x0 += s.dx();
    rect.inner_y0 += s.dy();
    rect.inner_x1 += s.dx();
    rect.inner_y1 += s.dy();
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
    // For large center slabs, split the draw into perimeter bands plus the
    // rectangular interior so the middle can be emitted as one fillRect.
    const Box& inner = rect.inner_mid;
    const Box top = Box::Intersect(
        Box(box.xMin(), box.yMin(), box.xMax(), inner.yMin() - 1), box);
    const Box left = Box::Intersect(
        Box(box.xMin(), inner.yMin(), inner.xMin() - 1, inner.yMax()), box);
    const Box clipped_inner = Box::Intersect(inner, box);
    const Box right = Box::Intersect(
        Box(inner.xMax() + 1, inner.yMin(), box.xMax(), inner.yMax()), box);
    const Box bottom = Box::Intersect(
        Box(box.xMin(), inner.yMax() + 1, box.xMax(), box.yMax()), box);
    if (!top.empty()) {
      FillSubrectangle(rect, spec, top);
    }
    if (!left.empty()) {
      FillSubrectangle(rect, spec, left);
    }
    if (!clipped_inner.empty() && (s.fill_mode() == FillMode::kExtents ||
                                   rect.interior_color != color::Transparent)) {
      s.out().fillRect(s.blending_mode(), clipped_inner,
                       spec.pre_blended_interior);
    }
    if (!right.empty()) {
      FillSubrectangle(rect, spec, right);
    }
    if (!bottom.empty()) {
      FillSubrectangle(rect, spec, bottom);
    }
  }
}

void DrawRoundRectCorners(SmoothShape::RoundRectCorners rect, const Surface& s,
                          const Box& box) {
  RoundRectDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .blending_mode = s.blending_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_outline = AlphaBlend(s.bgcolor(), rect.outline_color),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), rect.interior_color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    rect.x0 += s.dx();
    rect.y0 += s.dy();
    rect.x1 += s.dx();
    rect.y1 += s.dy();
    rect.inner_core = rect.inner_core.translate(s.dx(), s.dy());
    rect.top_slab = rect.top_slab.translate(s.dx(), s.dy());
    rect.bottom_slab = rect.bottom_slab.translate(s.dx(), s.dy());
    rect.left_slab = rect.left_slab.translate(s.dx(), s.dy());
    rect.right_slab = rect.right_slab.translate(s.dx(), s.dy());
  }
  if (box.area() <= 64) {
    FillSubrectOfRoundRectCorners(rect, spec, box);
    return;
  }
  if (rect.inner_core.empty() || rect.inner_core.width() <= 16) {
    FillSubrectangle(rect, spec, box);
    return;
  }

  const Box& inner = rect.inner_core;
  const Box top = Box::Intersect(
      Box(box.xMin(), box.yMin(), box.xMax(), inner.yMin() - 1), box);
  const Box left = Box::Intersect(
      Box(box.xMin(), inner.yMin(), inner.xMin() - 1, inner.yMax()), box);
  const Box clipped_inner = Box::Intersect(inner, box);
  const Box right = Box::Intersect(
      Box(inner.xMax() + 1, inner.yMin(), box.xMax(), inner.yMax()), box);
  const Box bottom = Box::Intersect(
      Box(box.xMin(), inner.yMax() + 1, box.xMax(), box.yMax()), box);
  if (!top.empty()) {
    FillSubrectangle(rect, spec, top);
  }
  if (!left.empty()) {
    FillSubrectangle(rect, spec, left);
  }
  if (!clipped_inner.empty() && (s.fill_mode() == FillMode::kExtents ||
                                 rect.interior_color != color::Transparent)) {
    s.out().fillRect(s.blending_mode(), clipped_inner,
                     spec.pre_blended_interior);
  }
  if (!right.empty()) {
    FillSubrectangle(rect, spec, right);
  }
  if (!bottom.empty()) {
    FillSubrectangle(rect, spec, bottom);
  }
}

}  // namespace

namespace internal {

round_rect::AreaType DetermineRectColorForRoundRect(
    const SmoothShape::RoundRect& rect, const Box& box) {
  return DetermineRectColorForRoundRectImpl(rect, box);
}

round_rect::AreaType DetermineRectColorForRoundRectCorners(
    const SmoothShape::RoundRectCorners& rect, const Box& box) {
  return DetermineRectColorForRoundRectCornersImpl(rect, box);
}

bool ReadColorRectOfRoundRect(const SmoothShape::RoundRect& rect, int16_t xMin,
                              int16_t yMin, int16_t xMax, int16_t yMax,
                              Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  // Readback mirrors the draw classifier: return one uniform color whenever a
  // tile can be proven exterior, interior, or outline without sampling.
  switch (DetermineRectColorForRoundRectImpl(rect, box)) {
    case round_rect::AreaType::kExterior: {
      *result = color::Transparent;
      return true;
    }
    case round_rect::AreaType::kInterior: {
      *result = rect.interior_color;
      return true;
    }
    case round_rect::AreaType::kOutline: {
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

bool ReadColorRectOfRoundRectCorners(const SmoothShape::RoundRectCorners& rect,
                                     int16_t xMin, int16_t yMin, int16_t xMax,
                                     int16_t yMax, Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForRoundRectCornersImpl(rect, box)) {
    case round_rect::AreaType::kExterior: {
      *result = color::Transparent;
      return true;
    }
    case round_rect::AreaType::kInterior: {
      *result = rect.interior_color;
      return true;
    }
    case round_rect::AreaType::kOutline: {
      *result = rect.outline_color;
      return true;
    }
    default:
      break;
  }
  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = PointInsideRoundRectCornersInteriorHelper(rect, x, y)
                   ? rect.interior_color
                   : GetSmoothRoundRectCornersPixelColor(rect, x, y);
    }
  }
  // Preserve the `readColorRect()` contract even on the mixed fallback path:
  // a single pixel or an accidentally uniform tile should still report true.
  const uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
  Color c = result[0];
  for (uint32_t i = 1; i < pixel_count; ++i) {
    if (result[i] != c) return false;
  }
  return true;
}

void ReadRoundRectColors(const SmoothShape::RoundRect& rect, const int16_t* x,
                         const int16_t* y, uint32_t count, Color* result) {
  while (count-- > 0) {
    if (PointInsideRoundRectInteriorHelper(rect, *x, *y)) {
      *result = rect.interior_color;
    } else {
      *result = GetSmoothRoundRectPixelColor(rect, *x, *y);
    }
    ++x;
    ++y;
    ++result;
  }
}

void ReadRoundRectCornersColors(const SmoothShape::RoundRectCorners& rect,
                                const int16_t* x, const int16_t* y,
                                uint32_t count, Color* result) {
  while (count-- > 0) {
    if (PointInsideRoundRectCornersInteriorHelper(rect, *x, *y)) {
      *result = rect.interior_color;
    } else {
      *result = GetSmoothRoundRectCornersPixelColor(rect, *x, *y);
    }
    ++x;
    ++y;
    ++result;
  }
}

std::unique_ptr<PixelStream> CreateRoundRectStream(
    const SmoothShape::RoundRect& rect, const Box& bounds) {
  // Keep the top-level stream selection simple; the specialization lives in
  // the stream implementation, not in the call sites.
  if (UsesRectInnerBoundary(rect)) {
    return std::unique_ptr<PixelStream>(
        new RectInnerRoundRectStream(rect, bounds));
  }
  return std::unique_ptr<PixelStream>(new RoundRectStream(rect, bounds));
}

std::unique_ptr<PixelStream> CreateRoundRectStream(
    const SmoothShape::RoundRectCorners& rect, const Box& bounds) {
  return std::unique_ptr<PixelStream>(new RoundRectCornersStream(rect, bounds));
}

void DrawRoundRect(SmoothShape::RoundRect rect, const Surface& s,
                   const Box& box) {
  DrawRoundRectImpl(rect, s, box);
}

void DrawRoundRectCorners(SmoothShape::RoundRectCorners rect, const Surface& s,
                          const Box& box) {
  ::roo_display::DrawRoundRectCorners(rect, s, box);
}

}  // namespace internal

}  // namespace roo_display
#pragma once

namespace roo_display {
namespace internal {

/// Normalized inner-boundary classification for a single-radius round rect.
enum class NormalizedRoundRectKind {
  kRoundInner = 0,
  kRectInner = 1,
  kFilled = 2,
};

/// Normalized outer and inner geometry derived from centerline round-rect data.
struct NormalizedSingleRadiusRoundRect {
  NormalizedRoundRectKind kind;
  float outer_x0;
  float outer_y0;
  float outer_x1;
  float outer_y1;
  float outer_radius;
  float inner_x0;
  float inner_y0;
  float inner_x1;
  float inner_y1;
  float inner_radius;
};

/// Normalizes single-radius centerline geometry for thick smooth round rects.
NormalizedSingleRadiusRoundRect NormalizeSingleRadiusRoundRect(
    float x0, float y0, float x1, float y1, float radius, float thickness);

}  // namespace internal
}  // namespace roo_display
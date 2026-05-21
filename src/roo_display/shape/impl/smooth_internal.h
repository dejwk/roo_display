#pragma once

#include "roo_display/shape/smooth.h"

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

/// Normalized centerline, outer, and inner geometry for a four-radii round
/// rect.
struct NormalizedFourRadiiRoundRect {
  bool filled;
  bool has_equal_centerline_radii;
  float centerline_x0;
  float centerline_y0;
  float centerline_x1;
  float centerline_y1;
  float thickness;
  float outer_x0;
  float outer_y0;
  float outer_x1;
  float outer_y1;
  float inner_x0;
  float inner_y0;
  float inner_x1;
  float inner_y1;
  RoundRectRadii centerline_radii;
  RoundRectRadii outer_radii;
  RoundRectRadii inner_radii;
};

/// Normalizes four-radii centerline geometry for thick smooth round rects.
NormalizedFourRadiiRoundRect NormalizeFourRadiiRoundRect(
    float x0, float y0, float x1, float y1, const RoundRectRadii& radii,
    float thickness);

}  // namespace internal
}  // namespace roo_display
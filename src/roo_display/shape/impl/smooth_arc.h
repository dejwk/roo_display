#pragma once

#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

namespace arc {

enum class AreaType {
  kMixed = 0,
  kExterior = 1,
  kInterior = 2,
  kOutlineActive = 3,
  kOutlineInactive = 4,
};

}  // namespace arc

bool ReadColorRectOfArc(const SmoothShape::Arc& arc, int16_t xMin, int16_t yMin,
                        int16_t xMax, int16_t yMax, Color* result);

arc::AreaType DetermineRectColorForArc(const SmoothShape::Arc& arc,
                                       const Box& box);

void ReadArcColors(const SmoothShape::Arc& arc, const int16_t* x,
                   const int16_t* y, uint32_t count, Color* result);

void DrawArc(SmoothShape::Arc arc, const Surface& s, const Box& box);

}  // namespace internal
}  // namespace roo_display
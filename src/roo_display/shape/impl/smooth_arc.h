#pragma once

#include "roo_display/shape/impl/smooth_rect_color.h"
#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

bool ReadColorRectOfArc(const SmoothShape::Arc& arc, int16_t xMin, int16_t yMin,
                        int16_t xMax, int16_t yMax, Color* result);

RectColor DetermineRectColorForArc(const SmoothShape::Arc& arc, const Box& box);

void ReadArcColors(const SmoothShape::Arc& arc, const int16_t* x,
                   const int16_t* y, uint32_t count, Color* result);

void DrawArc(SmoothShape::Arc arc, const Surface& s, const Box& box);

}  // namespace internal
}  // namespace roo_display
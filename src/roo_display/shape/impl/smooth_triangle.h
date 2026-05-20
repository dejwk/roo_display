#pragma once

#include "roo_display/shape/impl/smooth_rect_color.h"
#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

bool ReadColorRectOfTriangle(const SmoothShape::Triangle& triangle,
                             int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result);

RectColor DetermineRectColorForTriangle(const SmoothShape::Triangle& triangle,
                                        const Box& box);

void ReadTriangleColors(const SmoothShape::Triangle& triangle, const int16_t* x,
                        const int16_t* y, uint32_t count, Color* result);

void DrawTriangle(SmoothShape::Triangle triangle, const Surface& s,
                  const Box& box);

}  // namespace internal
}  // namespace roo_display
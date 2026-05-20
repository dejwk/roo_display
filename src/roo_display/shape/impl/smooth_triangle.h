#pragma once

#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

namespace triangle {

enum class AreaType {
  kMixed = 0,
  kExterior = 1,
  kInterior = 2,
};

}  // namespace triangle

bool ReadColorRectOfTriangle(const SmoothShape::Triangle& triangle,
                             int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result);

triangle::AreaType DetermineRectColorForTriangle(
    const SmoothShape::Triangle& triangle, const Box& box);

void ReadTriangleColors(const SmoothShape::Triangle& triangle, const int16_t* x,
                        const int16_t* y, uint32_t count, Color* result);

void DrawTriangle(SmoothShape::Triangle triangle, const Surface& s,
                  const Box& box);

}  // namespace internal
}  // namespace roo_display
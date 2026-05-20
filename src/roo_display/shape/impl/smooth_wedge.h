#pragma once

#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

void DrawWedge(const SmoothShape::Wedge& wedge, const Surface& s,
               const Box& box);

void ReadWedgeColors(const SmoothShape::Wedge& wedge, const int16_t* x,
                     const int16_t* y, uint32_t count, Color* result);

}  // namespace internal
}  // namespace roo_display
#pragma once

#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

void DrawPixel(SmoothShape::Pixel pixel, const Surface& s, const Box& box);

}  // namespace internal
}  // namespace roo_display
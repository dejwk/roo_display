#pragma once

#include "roo_display/shape/impl/smooth_rect_color.h"
#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

std::unique_ptr<PixelStream> CreateRoundRectStream(
    const SmoothShape::RoundRect& rect, const Box& bounds);

bool ReadColorRectOfRoundRect(const SmoothShape::RoundRect& rect, int16_t xMin,
                              int16_t yMin, int16_t xMax, int16_t yMax,
                              Color* result);

RectColor DetermineRectColorForRoundRect(const SmoothShape::RoundRect& rect,
                                         const Box& box);

void ReadRoundRectColors(const SmoothShape::RoundRect& rect, const int16_t* x,
                         const int16_t* y, uint32_t count, Color* result);

void DrawRoundRect(SmoothShape::RoundRect rect, const Surface& s,
                   const Box& box);

}  // namespace internal
}  // namespace roo_display
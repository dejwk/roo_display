#pragma once

#include "roo_display/shape/smooth.h"

namespace roo_display {
namespace internal {

namespace round_rect {

enum class AreaType {
  kMixed = 0,
  kExterior = 1,
  kInterior = 2,
  kOutline = 3,
};

}  // namespace round_rect

std::unique_ptr<PixelStream> CreateRoundRectStream(
    const SmoothShape::RoundRect& rect, const Box& bounds);

bool ReadColorRectOfRoundRect(const SmoothShape::RoundRect& rect, int16_t xMin,
                              int16_t yMin, int16_t xMax, int16_t yMax,
                              Color* result);

bool ReadColorRectOfRoundRectCorners(const SmoothShape::RoundRectCorners& rect,
                                     int16_t xMin, int16_t yMin, int16_t xMax,
                                     int16_t yMax, Color* result);

round_rect::AreaType DetermineRectColorForRoundRect(
    const SmoothShape::RoundRect& rect, const Box& box);

round_rect::AreaType DetermineRectColorForRoundRectCorners(
    const SmoothShape::RoundRectCorners& rect, const Box& box);

void ReadRoundRectColors(const SmoothShape::RoundRect& rect, const int16_t* x,
                         const int16_t* y, uint32_t count, Color* result);

void ReadRoundRectCornersColors(const SmoothShape::RoundRectCorners& rect,
                                const int16_t* x, const int16_t* y,
                                uint32_t count, Color* result);

void DrawRoundRect(SmoothShape::RoundRect rect, const Surface& s,
                   const Box& box);

void DrawRoundRectCorners(SmoothShape::RoundRectCorners rect, const Surface& s,
                          const Box& box);

}  // namespace internal
}  // namespace roo_display
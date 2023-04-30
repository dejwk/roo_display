#include "roo_display/shape/shadow.h"

namespace roo_display {

namespace {

inline uint16_t isqrt32(uint32_t x) {
  uint32_t r = 0, r2 = 0;
  for (int p = 15; p >= 0; --p) {
    uint32_t tr2 = r2 + (r << (p + 1)) + ((uint32_t)1u << (p + p));
    if (tr2 <= x) {
      r2 = tr2;
      r |= ((uint32_t)1u << p);
    }
  }
  return r;
}

// Returns the diffusion of the shadow at the specified (x, y) pixel within a
// shadow given by the spec. Returned value is from 0 (no diffusion) to 16 *
// extents.
inline uint16_t calcShadowDiffusion(const RoundRectShadow::Spec& spec,
                                    int16_t x, int16_t y) {
  x -= spec.x;
  y -= spec.y;

  // We need to consider 9 cases:
  //
  // 123
  // 456
  // 789
  //
  // We will fold them to
  //
  // 12
  // 34
  if (x >= spec.w - spec.radius) {
    x += spec.radius - spec.w + 1;
  } else {
    x = spec.radius - x;
  }
  if (y >= spec.h - spec.radius) {
    y += spec.radius - spec.h + 1;
  } else {
    y = spec.radius - y;
  }
  if (x <= 0) {
    if (y > 0) return 16 * y;  // Case 2.
    return 0;                  // Case 4.
  }
  if (y <= 0) return 16 * x;  // Case 3.
  // Now what's left is Case 1.
  return isqrt32(256 * (x * x + y * y));
}

// Calculates alpha component of the specified (x, y) point within a shadow
// given by the spec.
inline uint8_t calcShadowAlpha(const RoundRectShadow::Spec& spec, int16_t x,
                               int16_t y) {
  uint16_t d = calcShadowDiffusion(spec, x, y);
  if (d > spec.border * 16) {
    if (d > spec.radius * 16) {
      return 0;
    } else {
      return spec.alpha_start -
             ((uint32_t)((d - spec.border * 16) * spec.alpha_step) / 256 / 16);
    }
  }
  return spec.alpha_start;
}

}  // namespace

RoundRectShadow::RoundRectShadow(roo_display::Box extents, Color color,
                                 uint8_t blur_radius, uint8_t dx, uint8_t dy,
                                 uint8_t corner_radius)
    : object_extents_(extents), corner_radius_(corner_radius), color_(color) {
  spec_.radius = blur_radius + corner_radius;
  spec_.x = extents.xMin() - blur_radius + dx;
  spec_.y = extents.yMin() - blur_radius + dy;
  spec_.w = extents.width() + 2 * blur_radius;
  spec_.h = extents.height() + 2 * blur_radius;
  spec_.alpha_start = color_.a();
  spec_.alpha_step =
      (blur_radius == 0) ? 0 : 256 * spec_.alpha_start / blur_radius;
  spec_.border = corner_radius;

  shadow_extents_ =
      Box(spec_.x, spec_.y, spec_.x + spec_.w - 1, spec_.y + spec_.h - 1);
}

void RoundRectShadow::readColors(const int16_t* x, const int16_t* y,
                                 uint32_t count, Color* result) const {
  while (count-- > 0) {
    *result++ = color_.withA(calcShadowAlpha(spec_, *x++, *y++));
  }
}

bool RoundRectShadow::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                    int16_t yMax,
                                    roo_display::Color* result) const {
  if (xMin >= object_extents_.xMin() + corner_radius_ &&
      xMax <= object_extents_.xMax() - corner_radius_) {
    if (yMin >= object_extents_.yMin() && yMax <= object_extents_.yMax()) {
      // Interior of the shadow.
      *result = color_;
      return true;
    }
    // Pixel color in this range does not depend on the specific x value at
    // all, so we can compute only one vertical stripe and replicate it.
    for (int16_t y = yMin; y <= yMax; ++y) {
      Color c = color_.withA(calcShadowAlpha(spec_, xMin, y));
      for (int16_t x = xMin; x <= xMax; ++x) {
        *result++ = c;
      }
    }
    return false;
  } else if (yMin >= object_extents_.yMin() + corner_radius_ &&
             yMax <= object_extents_.yMax() - corner_radius_) {
    // Pixel color in this range does not depend on the specific y value
    // at all, so we can compute only one horizontal stripe and replicate it.
    for (int16_t x = xMin; x <= xMax; ++x) {
      Color c = color_.withA(calcShadowAlpha(spec_, x, yMin));
      Color* ptr = result++;
      int16_t stride = (xMax - xMin + 1);
      for (int16_t y = yMin; y <= yMax; ++y) {
        *ptr = c;
        ptr += stride;
      }
    }
    return false;
  }
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *result++ = color_.withA(calcShadowAlpha(spec_, x, y));
    }
  }
  return false;
}

}  // namespace roo_display

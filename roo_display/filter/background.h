#pragma once

#include "roo_display/filter/blender.h"

namespace roo_display {

namespace internal {

struct BgBlendKernel {
  Color operator()(Color c, Color raster) const {
    return AlphaBlend(raster, c);
  }
};

}  // namespace internal

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' background.
using BackgroundFilter = BlendingFilter<internal::BgBlendKernel>;

}  // namespace roo_display
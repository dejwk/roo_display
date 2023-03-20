#pragma once

#include "roo_display/filter/blender.h"

namespace roo_display {

namespace internal {

struct FgBlendKernel {
  Color operator()(Color c, Color raster) const {
    return AlphaBlend(c, raster);
  }
  Color bgcolor() { return color::Transparent; }
};

}  // namespace internal

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' foreground.
using ForegroundFilter = BlendingFilter<internal::FgBlendKernel>;

}  // namespace roo_display
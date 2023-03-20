#pragma once

#include "roo_display/filter/blender.h"

namespace roo_display {

namespace internal {

struct BgBlendKernel {
  Color operator()(Color c, Color raster) const {
    return AlphaBlend(raster, c);
  }
  Color bgcolor() const { return color::Transparent; }
};

struct BgBlendKernelWithColor {
  Color operator()(Color c, Color raster) const {
    return AlphaBlend(bg, AlphaBlend(raster, c));
  }
  Color bgcolor() const { return bg; }

  Color bg;
};

}  // namespace internal

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' background.
using BackgroundFilter = BlendingFilter<internal::BgBlendKernel>;

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' background and an underlying
// background color.
using BackgroundFilterWithColor = BlendingFilter<internal::BgBlendKernelWithColor>;

}  // namespace roo_display
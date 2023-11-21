#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/filter/blender.h"

namespace roo_display {

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' background.
using BackgroundFilter = BlendingFilter<BlendOp<BLENDING_MODE_SOURCE_OVER>>;

}  // namespace roo_display
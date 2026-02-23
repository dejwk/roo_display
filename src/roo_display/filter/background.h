#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/filter/blender.h"

namespace roo_display {

/// Filtering device that blends with a rasterizable background.
using BackgroundFilter = BlendingFilter<BlendOp<kBlendingSourceOver>>;

}  // namespace roo_display
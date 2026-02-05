#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/filter/blender.h"

namespace roo_display {

/// Filtering device that blends with a rasterizable foreground.
using ForegroundFilter =
    BlendingFilter<BlendOp<BLENDING_MODE_DESTINATION_OVER>>;

}  // namespace roo_display
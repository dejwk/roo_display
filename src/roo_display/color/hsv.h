#pragma once

#include "roo_display/color/color.h"

namespace roo_display {

/// Convert HSV (h, s, v) to an RGB `Color`.
Color HsvToRgb(float h, float s, float v);

}  // namespace roo_display
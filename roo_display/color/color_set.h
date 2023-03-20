#pragma once

#include "roo_display/color/color.h"
#include "roo_display/internal/hashtable.h"

namespace roo_display {

namespace internal {

struct ColorHash {
  uint32_t operator()(Color color) const { return color.asArgb(); }
};

typedef HashSet<Color, ColorHash> ColorSet;

}  // namespace internal

}  // namespace roo_display

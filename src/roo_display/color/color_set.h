#pragma once

#include "roo_collections/flat_small_hash_set.h"
#include "roo_display/color/color.h"

namespace roo_display {

namespace internal {

struct ColorHash {
  uint32_t operator()(Color color) const { return color.asArgb(); }
};

typedef roo_collections::FlatSmallHashSet<Color, ColorHash> ColorSet;

}  // namespace internal

}  // namespace roo_display

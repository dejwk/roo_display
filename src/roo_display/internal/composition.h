#pragma once

#include "roo_display/color/blending.h"

namespace roo_display {
namespace internal {

// Classifies composition modes that reset the accumulated color to Transparent
// outside a source's clipped extents. Other modes leave it unchanged. This is
// the absent-source rule, not blending an explicit alpha-zero sample (which
// can produce Background or retain RGB).
inline bool IsAbsentSourceClearing(BlendingMode mode) {
  switch (mode) {
    case BlendingMode::kSource:
    case BlendingMode::kSourceIn:
    case BlendingMode::kSourceOut:
    case BlendingMode::kDestinationIn:
    case BlendingMode::kDestinationAtop:
    case BlendingMode::kClear:
      return true;
    default:
      return false;
  }
}

}  // namespace internal
}  // namespace roo_display

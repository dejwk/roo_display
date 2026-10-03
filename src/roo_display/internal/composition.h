#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/core/box.h"

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

// Proves whole-stack opacity without sampling pixels or caching mutable source
// metadata. Partial coverage cannot establish opacity, and absent masks still
// clear the accumulated result. Unknown cases retain the safe full-alpha hint.
template <typename Inputs>
TransparencyMode CompositionTransparency(const Inputs& inputs,
                                         const Box& bounds) {
  if (bounds.empty()) return TransparencyMode::kFull;
  bool opaque = false;
  for (const auto& input : inputs) {
    BlendingMode mode = input.blending_mode();
    if (Box::Intersect(input.extents(), bounds).empty()) {
      if (IsAbsentSourceClearing(mode)) opaque = false;
      continue;
    }
    switch (mode) {
      case BlendingMode::kDestination:
      case BlendingMode::kSourceAtop:
        break;
      case BlendingMode::kSourceOver:
      case BlendingMode::kDestinationOver:
        opaque = opaque || (input.extents().contains(bounds) &&
                            input.source()->getTransparencyMode() ==
                                TransparencyMode::kNone);
        break;
      case BlendingMode::kSource:
      case BlendingMode::kDestinationAtop:
        opaque =
            input.extents().contains(bounds) &&
            input.source()->getTransparencyMode() == TransparencyMode::kNone;
        break;
      case BlendingMode::kSourceIn:
      case BlendingMode::kDestinationIn:
        opaque =
            opaque && input.extents().contains(bounds) &&
            input.source()->getTransparencyMode() == TransparencyMode::kNone;
        break;
      default:
        opaque = false;
        break;
    }
  }
  return opaque ? TransparencyMode::kNone : TransparencyMode::kFull;
}

}  // namespace internal
}  // namespace roo_display

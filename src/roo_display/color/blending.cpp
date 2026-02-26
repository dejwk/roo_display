#include "roo_display/color/blending.h"

namespace roo_display {
namespace {

const char* ToString(BlendingMode mode) {
  switch (mode) {
    case BlendingMode::kSource:
      return "BlendingMode::kSource";
    case BlendingMode::kSourceOver:
      return "BlendingMode::kSourceOver";
    case BlendingMode::kSourceIn:
      return "BlendingMode::kSourceIn";
    case BlendingMode::kSourceAtop:
      return "BlendingMode::kSourceAtop";
    case BlendingMode::kDestination:
      return "BlendingMode::kDestination";
    case BlendingMode::kDestinationOver:
      return "BlendingMode::kDestinationOver";
    case BlendingMode::kDestinationIn:
      return "BlendingMode::kDestinationIn";
    case BlendingMode::kDestinationAtop:
      return "BlendingMode::kDestinationAtop";
    case BlendingMode::kClear:
      return "BlendingMode::kClear";
    case BlendingMode::kSourceOut:
      return "BlendingMode::kSourceOut";
    case BlendingMode::kDestinationOut:
      return "BlendingMode::kDestinationOut";
    case BlendingMode::kXor:
      return "BlendingMode::kXor";
    case BlendingMode::kSourceOverOpaque:
      return "BlendingMode::kSourceOverOpaque";
    case BlendingMode::kDestinationOverOpaque:
      return "BlendingMode::kDestinationOverOpaque";
  }
  return "BlendingMode::kUnknown";
}

const char* ToString(TransparencyMode mode) {
  switch (mode) {
    case TransparencyMode::kNone:
      return "TransparencyMode::kNone";
    case TransparencyMode::kCrude:
      return "TransparencyMode::kCrude";
    case TransparencyMode::kFull:
      return "TransparencyMode::kFull";
  }
  return "TransparencyMode::kUnknown";
}

}  // namespace

roo_logging::Stream& operator<<(roo_logging::Stream& os, BlendingMode mode) {
  return os << ToString(mode);
}

roo_logging::Stream& operator<<(roo_logging::Stream& os,
                                TransparencyMode mode) {
  return os << ToString(mode);
}

}  // namespace roo_display

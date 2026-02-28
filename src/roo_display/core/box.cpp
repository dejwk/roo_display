#include "roo_display/core/box.h"

namespace roo_display {

roo_logging::Stream& operator<<(roo_logging::Stream& os, const Box& box) {
  os << "[" << box.xMin() << ", " << box.yMin() << ", " << box.xMax() << ", "
     << box.yMax() << "]";
  return os;
}

roo_logging::Stream& operator<<(roo_logging::Stream& os,
                                Box::ClipResult clip_result) {
  switch (clip_result) {
    case Box::ClipResult::kEmpty:
      os << "Box::ClipResult::kEmpty";
      break;
    case Box::ClipResult::kReduced:
      os << "Box::ClipResult::kReduced";
      break;
    case Box::ClipResult::kUnchanged:
      os << "Box::ClipResult::kUnchanged";
      break;
    default:
      os << "Box::ClipResult::(unknown)";
      break;
  }
  return os;
}

}  // namespace roo_display
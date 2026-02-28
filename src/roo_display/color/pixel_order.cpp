#include "roo_display/color/pixel_order.h"

namespace roo_display {

namespace {

const char* ToString(ColorPixelOrder order) {
  switch (order) {
    case ColorPixelOrder::kMsbFirst:
      return "ColorPixelOrder::kMsbFirst";
    case ColorPixelOrder::kLsbFirst:
      return "ColorPixelOrder::kLsbFirst";
  }
  return "ColorPixelOrder::(unknown)";
}

}  // namespace

roo_logging::Stream& operator<<(roo_logging::Stream& os,
                                ColorPixelOrder order) {
  return os << ToString(order);
}

}  // namespace roo_display

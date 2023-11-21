#include <utility>
#include <vector>

#include "color_gradient.h"

namespace roo_display {

Color ColorGradient::getColor(double value) const {
  // First, find the matching interval.
  uint16_t right_bound = 0;
  while (right_bound < gradient_.size() &&
         gradient_[right_bound].first < value) {
    right_bound++;
  }
  if (right_bound == 0) {
    return gradient_[0].second;
  } else if (right_bound >= gradient_.size()) {
    return gradient_.back().second;
  } else {
    // Then, interpolate.
    double left_temp = gradient_[right_bound - 1].first;
    double right_temp = gradient_[right_bound].first;
    int16_t f = (int16_t)(256 * (value - left_temp) / (right_temp - left_temp));
    Color left = gradient_[right_bound - 1].second;
    Color right = gradient_[right_bound].second;
    uint32_t a =
        ((uint16_t)left.a() * (256 - f) + (uint16_t)right.a() * f) / 256;
    uint32_t r =
        ((uint16_t)left.r() * (256 - f) + (uint16_t)right.r() * f) / 256;
    uint32_t g =
        ((uint16_t)left.g() * (256 - f) + (uint16_t)right.g() * f) / 256;
    uint32_t b =
        ((uint16_t)left.b() * (256 - f) + (uint16_t)right.b() * f) / 256;
    return Color(a << 24 | r << 16 | g << 8 | b);
  }
}

}  // namespace

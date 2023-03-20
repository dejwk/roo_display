#pragma once

#include <initializer_list>
#include <utility>
#include <vector>

#include "roo_display/color/color.h"

namespace roo_display {

// Given the list of colors at a handful of (at least two) specified
// floating-point values, dynamically generates colors for arbitrary
// floating-point values, using linear interpolation.
class ColorGradient {
 public:
  // The gradient argument must be sorted by the first (double) element.
  ColorGradient(
      const std::initializer_list<std::pair<double, roo_display::Color>>&
          gradient)
      : gradient_(gradient) {}

  roo_display::Color getColor(double value) const;

 private:
  std::vector<std::pair<double, roo_display::Color>> gradient_;
};

}  // namespace roo_display

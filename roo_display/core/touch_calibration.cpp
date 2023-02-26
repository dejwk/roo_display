#include "roo_display/core/touch_calibration.h"

#include "roo_display/core/box.h"

namespace roo_display {

void TouchCalibration::Calibrate(int16_t& x, int16_t& y, int16_t& z) {
  if (orientation_.isXYswapped()) {
    std::swap(x, y);
  }
  if (x < bounds_.xMin()) x = bounds_.xMin();
  if (x > bounds_.xMax()) x = bounds_.xMax();
  if (y < bounds_.yMin()) y = bounds_.yMin();
  if (y > bounds_.yMax()) y = bounds_.yMax();
  x = (int32_t)4096 * (x - bounds_.xMin()) / bounds_.width();
  y = (int32_t)4096 * (y - bounds_.yMin()) / bounds_.height();
  if (orientation_.isRightToLeft()) {
    x = 4095 - x;
  }
  if (orientation_.isBottomToTop()) {
    y = 4095 - y;
  }
}

}  // namespace roo_display
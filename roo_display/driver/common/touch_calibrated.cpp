#include "touch_calibrated.h"

#include "roo_display/core/box.h"

namespace roo_display {

bool TouchCalibration::Calibrate(int16_t* x, int16_t* y, int16_t* z) {
  if (orientation_.isRightToLeft()) {
    *x = 4096 - *x;
  }
  if (orientation_.isBottomToTop()) {
    *y = 4096 - *y;
  }
  if (orientation_.isXYswapped()) {
    std::swap(*x, *y);
  }
  if (*x < bounds_.xMin()) *x = bounds_.xMin();
  if (*x > bounds_.xMax()) *x = bounds_.xMax();
  if (*y < bounds_.yMin()) *x = bounds_.yMin();
  if (*y > bounds_.yMax()) *x = bounds_.yMax();
  *x = (int32_t)4096 * (*x - bounds_.xMin()) / bounds_.width();
  *y = (int32_t)4096 * (*y - bounds_.yMin()) / bounds_.height();
  return true;
}

bool TouchCalibrated::getTouch(int16_t* x, int16_t* y, int16_t* z) {
  bool touched = touch_->getTouch(x, y, z);
  return touched ? calibration_.Calibrate(x, y, z) : false;
}

}  // namespace roo_display

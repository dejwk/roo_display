#include "roo_display/touch/calibration.h"

#include "roo_display/core/box.h"

namespace roo_display {

void TouchCalibration::Calibrate(TouchPoint& p) {
  if (orientation_.isXYswapped()) {
    std::swap(p.x, p.y);
    std::swap(p.vx, p.vy);
  }
  if (p.x < bounds_.xMin()) p.x = bounds_.xMin();
  if (p.x > bounds_.xMax()) p.x = bounds_.xMax();
  if (p.y < bounds_.yMin()) p.y = bounds_.yMin();
  if (p.y > bounds_.yMax()) p.y = bounds_.yMax();
  int16_t w = bounds_.width();
  int16_t h = bounds_.height();
  p.x = ((int32_t)4096 * (p.x - bounds_.xMin()) + w / 2) / w;
  p.y = ((int32_t)4096 * (p.y - bounds_.yMin()) + h / 2) / h;
  p.vx = ((int32_t)4096 * p.vx + w / 2) / w;
  p.vy = ((int32_t)4096 * p.vy + h / 2) / h;
  if (orientation_.isRightToLeft()) {
    p.x = 4095 - p.x;
    p.vx = -p.vx;
  }
  if (orientation_.isBottomToTop()) {
    p.y = 4095 - p.y;
    p.vy = -p.vy;
  }
}

}  // namespace roo_display
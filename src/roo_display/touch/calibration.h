#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"

namespace roo_display {

class TouchCalibration {
 public:
  // Effectively uncalibrated, put possibly reoriented.
  constexpr TouchCalibration(Orientation orientation = Orientation::Default())
      : TouchCalibration(Box(0, 0, 4095, 4095), orientation) {}

  // Calibrated and possibly reoriented.
  constexpr TouchCalibration(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             Orientation orientation = Orientation::Default())
      : TouchCalibration(Box(x0, y0, x1, y1), orientation) {}

  // Calibrated and possibly reoriented.
  constexpr TouchCalibration(Box bounds,
                             Orientation orientation = Orientation::Default())
      : bounds_(std::move(bounds)), orientation_(orientation) {}

  // Augments the touch point according to the calibration spec.
  void Calibrate(TouchPoint& point);

  const Box& bounds() const { return bounds_; }
  Orientation orientation() const { return orientation_; }

 private:
  Box bounds_;
  Orientation orientation_;
};

}  // namespace roo_display

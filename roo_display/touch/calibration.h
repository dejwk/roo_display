#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/orientation.h"

namespace roo_display {

class TouchCalibration {
 public:
  // Effectively uncalibrated, put possibly reoriented.
  TouchCalibration(Orientation orientation = Orientation::Default())
      : TouchCalibration(Box(0, 0, 4095, 4095), orientation) {}

  // Calibrated and possibly reoriented.
  TouchCalibration(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   Orientation orientation = Orientation::Default())
      : TouchCalibration(Box(x0, y0, x1, y1), orientation) {}

  // Calibrated and possibly reoriented.
  TouchCalibration(Box bounds, Orientation orientation = Orientation::Default())
      : bounds_(std::move(bounds)), orientation_(orientation) {}

  // Augments x and y so that they are within range [0, 4095], and
  // correspond to the logical {x, y} direction of the calibrated device.
  void Calibrate(int16_t& x, int16_t& y, int16_t& z);

 private:
  Box bounds_;
  Orientation orientation_;
};

}  // namespace roo_display

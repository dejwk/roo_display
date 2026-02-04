#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"

namespace roo_display {

/// Touch calibration parameters (bounds + orientation).
class TouchCalibration {
 public:
    /// Uncalibrated (0..4095) bounds with optional orientation.
  constexpr TouchCalibration(Orientation orientation = Orientation::Default())
      : TouchCalibration(Box(0, 0, 4095, 4095), orientation) {}

    /// Calibrated bounds with optional orientation.
  constexpr TouchCalibration(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             Orientation orientation = Orientation::Default())
      : TouchCalibration(Box(x0, y0, x1, y1), orientation) {}

    /// Calibrated bounds with optional orientation.
  constexpr TouchCalibration(Box bounds,
                             Orientation orientation = Orientation::Default())
      : bounds_(std::move(bounds)), orientation_(orientation) {}

    /// Apply calibration and orientation to the given touch point.
  void Calibrate(TouchPoint& point);

    /// Return calibration bounds.
  const Box& bounds() const { return bounds_; }
    /// Return calibration orientation.
  Orientation orientation() const { return orientation_; }

 private:
  Box bounds_;
  Orientation orientation_;
};

}  // namespace roo_display

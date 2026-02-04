#pragma once

#include "roo_display/core/device.h"
#include "roo_display/touch/calibration.h"

namespace roo_display {

/// Base class for displays with integrated, pre-calibrated touch.
class ComboDevice {
 public:
  virtual ~ComboDevice() {}

  /// Return the display device.
  virtual DisplayDevice& display() = 0;

  /// Return the touch device (or nullptr if absent).
  virtual TouchDevice* touch() { return nullptr; }

  /// Return touch calibration.
  virtual TouchCalibration touch_calibration() { return TouchCalibration(); }
};

}  // namespace roo_display
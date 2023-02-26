#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/touch_calibration.h"

namespace roo_display {

// Base class for displays with integrated, pre-calibrated touch.
class ComboDevice {
 public:
  virtual ~ComboDevice() {}

  virtual DisplayDevice& display() = 0;

  virtual TouchDevice* touch() { return nullptr; }

  virtual TouchCalibration touch_calibration() { return TouchCalibration(); }
};

}  // namespace roo_display
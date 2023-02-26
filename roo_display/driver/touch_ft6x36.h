#pragma once

#include "roo_display/core/device.h"

namespace roo_display {

class TouchFt6x36 : public TouchDevice {
 public:
  TouchFt6x36() {}

  bool getTouch(int16_t &x, int16_t &y, int16_t &z) override;
};

}  // namespace roo_display

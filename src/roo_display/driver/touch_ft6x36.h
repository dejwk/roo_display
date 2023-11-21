#pragma once

#include <Wire.h>

#include "roo_display/driver/common/basic_touch.h"

namespace roo_display {

class TouchFt6x36 : public BasicTouchDevice<2> {
 public:
  TouchFt6x36();
  TouchFt6x36(decltype(Wire)& wire);

  int readTouch(TouchPoint* point) override;

 private:
  decltype(Wire)& wire_;
};

}  // namespace roo_display

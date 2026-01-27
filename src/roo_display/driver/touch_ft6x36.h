#pragma once

#include "roo_display/driver/common/basic_touch.h"
#include "roo_display/hal/i2c.h"

namespace roo_display {

class TouchFt6x36 : public BasicTouchDevice<2> {
 public:
  TouchFt6x36(I2cMasterBusHandle i2c);
  TouchFt6x36();

  int readTouch(TouchPoint* point) override;

 private:
  I2cSlaveDevice i2c_slave_;
};

}  // namespace roo_display

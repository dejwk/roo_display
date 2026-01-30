#pragma once

#include "roo_display/driver/common/basic_touch.h"
#include "roo_display/hal/i2c.h"

namespace roo_display {

class TouchFt6x36 : public BasicTouchDevice<2> {
 public:
  // Constructs the driver assuming the default I2C bus.
  TouchFt6x36();

  // Constructs the driver using the specified I2C bus (passed as the first
  // argument).
  // When using Arduino, you can pass a Wire& object reference.
  // When using esp-idf, you can pass an i2c_port_num_t.
  TouchFt6x36(I2cMasterBusHandle i2c);

  // Initializes the driver (performing GPIO setup and calling reset()).
  // Must be called once.
  void initTouch() override;

  int readTouch(TouchPoint* point) override;

 private:
  I2cSlaveDevice i2c_slave_;
};

}  // namespace roo_display

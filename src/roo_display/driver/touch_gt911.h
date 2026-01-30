#pragma once

#include "roo_display/core/device.h"
#include "roo_display/driver/common/basic_touch.h"
#include "roo_display/hal/i2c.h"
#include "roo_threads.h"
#include "roo_threads/atomic.h"
#include "roo_threads/thread.h"

namespace roo_display {

class TouchGt911 : public BasicTouchDevice<5> {
 public:
  // Constructs the driver assuming the default I2C bus.
  TouchGt911(int8_t pinIntr, int8_t pinRst, long reset_low_hold_ms = 1);

  // Constructs the driver using the specified I2C bus (passed as the first
  // argument).
  // When using Arduino, you can pass a Wire& object reference.
  // When using esp-idf, you can pass an i2c_port_num_t.
  TouchGt911(I2cMasterBusHandle i2c, int8_t pinIntr, int8_t pinRst,
             long reset_low_hold_ms = 1);

  // Initializes the driver (performing GPIO setup and calling reset()).
  // Must be called once.
  void initTouch() override;

  int readTouch(TouchPoint* point) override;

  void reset();

 private:
  bool readByte(uint16_t reg, roo::byte& result);
  void writeByte(uint16_t reg, roo::byte val);
  void readBlock(roo::byte* buf, uint16_t reg, uint8_t size);

  int8_t addr_;
  int8_t pinIntr_;
  int8_t pinRst_;
  I2cSlaveDevice i2c_slave_;
  long reset_low_hold_ms_;

  roo::atomic<bool> ready_;
  roo::thread reset_thread_;
};

}  // namespace roo_display
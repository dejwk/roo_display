#pragma once

#include <Wire.h>

#include <atomic>

#include "roo_display/core/device.h"
#include "roo_display/driver/common/basic_touch.h"
#include "roo_threads.h"
#include "roo_threads/thread.h"

namespace roo_display {

class TouchGt911 : public BasicTouchDevice<5> {
 public:
  TouchGt911(int8_t pinIntr, int8_t pinRst)
      : TouchGt911(Wire, pinIntr, pinRst) {}

  TouchGt911(decltype(Wire)& wire, int8_t pinIntr, int8_t pinRst,
             long reset_low_hold_ms = 1);

  void initTouch() override;

  int readTouch(TouchPoint* point) override;

  void reset();

 private:
  bool readByte(uint16_t reg, uint8_t& result);
  void writeByte(uint16_t reg, uint8_t val);
  void readBlock(uint8_t* buf, uint16_t reg, uint8_t size);
  void writeBlock(uint16_t reg, const uint8_t* val, uint8_t size);

  int8_t addr_;
  int8_t pinIntr_;
  int8_t pinRst_;
  decltype(Wire)& wire_;
  long reset_low_hold_ms_;

  std::atomic<bool> ready_;
  roo::thread reset_thread_;
};

}  // namespace roo_display
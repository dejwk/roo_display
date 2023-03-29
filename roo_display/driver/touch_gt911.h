#pragma once

#include "roo_display/core/device.h"
#include "roo_display/driver/common/basic_touch.h"

#include <Wire.h>

namespace roo_display {

class TouchGt911 : public BasicTouchDevice<5> {
 public:
  TouchGt911(int8_t pinIntr, int8_t pinRst)
      : TouchGt911(Wire, pinIntr, pinRst) {}

  TouchGt911(decltype(Wire)& wire, int8_t pinIntr, int8_t pinRst);

  void initTouch() override;

  int readTouch(TouchPoint* point) override;

  void reset();

 private:
  uint8_t readByte(uint16_t reg);
  void writeByte(uint16_t reg, uint8_t val);
  void readBlock(uint8_t* buf, uint16_t reg, uint8_t size);
  void writeBlock(uint16_t reg, const uint8_t* val, uint8_t size);

  uint8_t addr_;
  uint8_t pinIntr_;
  uint8_t pinRst_;
  decltype(Wire)& wire_;
};

}  // namespace roo_display
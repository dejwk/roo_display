#include "roo_display/driver/touch_ft6x36.h"

#include <Wire.h>

namespace roo_display {

namespace {

static const int kTouchI2cAdd = 0x38;
static const int kTouchRegXl = 0x04;
static const int kTouchRegXh = 0x03;
static const int kTouchRegYl = 0x06;
static const int kTouchRegYh = 0x05;

int readTouchReg(int reg) {
  int data = 0;
  Wire.beginTransmission(kTouchI2cAdd);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(kTouchI2cAdd, 1);
  if (Wire.available()) {
    data = Wire.read();
  } else {
    return 255;
  }
  return data;
}

int getTouchPointX() {
  int xh = readTouchReg(kTouchRegXh);
  if (xh >> 6 == 1) return -1;
  int xl = readTouchReg(kTouchRegXl);
  return ((xh & 0x0F) << 8) | xl;
}

int getTouchPointY() {
  int yh = readTouchReg(kTouchRegYh);
  int yl = readTouchReg(kTouchRegYl);
  return ((yh & 0x0F) << 8) | yl;
}

}  // namespace

bool TouchFt6x36::getTouch(int16_t &x, int16_t &y, int16_t &z) {
  int tx = getTouchPointX();
  if (tx <= 0) return false;
  int ty = getTouchPointY();
  if (ty <= 0) return false;
  x = tx;
  y = ty;
  z = 500;
  return true;
}

}  // namespace roo_display

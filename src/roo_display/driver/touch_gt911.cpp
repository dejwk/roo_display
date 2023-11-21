#include "roo_display/driver/touch_gt911.h"

#include <Arduino.h>
#include <Wire.h>

#include "roo_display/core/device.h"

namespace roo_display {

namespace {

static constexpr int kAddr1 = 0x5D;
static constexpr int kAddr2 = 0x14;

static constexpr int kTouchRead = 0x814e;
static constexpr int kPointBuffer = 0x814f;

TouchPoint ReadPoint(const uint8_t* data) {
  TouchPoint tp;
  tp.id = data[0];
  tp.x = (data[2] << 8) | data[1];
  tp.y = (data[4] << 8) | data[3];
  tp.z = 1024;
  return tp;
}

}  // namespace

void TouchGt911::initTouch() { reset(); }

TouchGt911::TouchGt911(decltype(Wire)& wire, int8_t pinIntr, int8_t pinRst)
    : BasicTouchDevice<5>(Config{.min_sampling_interval_ms = 20,
                                 .touch_intertia_ms = 30,
                                 .smoothing_factor = 0.0}),
      addr_(kAddr1),
      pinIntr_(pinIntr),
      pinRst_(pinRst),
      wire_(wire) {}

void TouchGt911::reset() {
  pinMode(pinIntr_, OUTPUT);
  pinMode(pinRst_, OUTPUT);
  digitalWrite(pinRst_, 0);
  digitalWrite(pinIntr_, 0);
  delay(10);
  digitalWrite(pinIntr_, addr_ == kAddr2);
  delay(1);
  digitalWrite(pinRst_, 1);
  delay(5);
  digitalWrite(pinIntr_, 0);
  // Note: the programming guide for 911 says that it is at least 50ms until the
  // interrupts start working, and up to 100 ms before scan starts clocking, but
  // it seems to be no need to block for that here - the worst that can happen
  // is that the touch will not be detected for that initial period.
}

int TouchGt911::readTouch(TouchPoint* points) {
  uint8_t status = readByte(kTouchRead);
  uint8_t ready = (status & 0x80) != 0;
  // uint8_t have_key = (status & 0x10) != 0;
  if (!ready) return 0;
  int touches = status & 0xF;
  uint8_t data[7];
  for (uint8_t i = 0; i < touches; i++) {
    readBlock(data, kPointBuffer + i * 8, 7);
    points[i] = ReadPoint(data);
  }
  // Clear the 'ready' flag.
  writeByte(kTouchRead, 0);
  return touches;
}

uint8_t TouchGt911::readByte(uint16_t reg) {
  uint8_t x;
  wire_.beginTransmission(addr_);
  wire_.write(reg >> 8);
  wire_.write(reg & 0xFF);
  wire_.endTransmission();
  wire_.requestFrom(addr_, (uint8_t)1);
  x = wire_.read();
  return x;
}

void TouchGt911::writeByte(uint16_t reg, uint8_t val) {
  wire_.beginTransmission(addr_);
  wire_.write(reg >> 8);
  wire_.write(reg & 0xFF);
  wire_.write(val);
  wire_.endTransmission();
}

void TouchGt911::readBlock(uint8_t* buf, uint16_t reg, uint8_t size) {
  wire_.beginTransmission(addr_);
  wire_.write(reg >> 8);
  wire_.write(reg & 0xFF);
  wire_.endTransmission();
  wire_.requestFrom(addr_, size);
  for (int i = 0; i < size; i++) {
    buf[i] = wire_.read();
  }
}

void TouchGt911::writeBlock(uint16_t reg, const uint8_t* val, uint8_t size) {
  wire_.beginTransmission(addr_);
  wire_.write(reg >> 8);
  wire_.write(reg & 0xFF);
  for (int i = 0; i < size; i++) {
    wire_.write(val[i]);
  }
  wire_.endTransmission();
}

}  // namespace roo_display

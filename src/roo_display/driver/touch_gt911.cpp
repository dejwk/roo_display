#include "roo_display/driver/touch_gt911.h"

#include <Arduino.h>

#include "roo_display/core/device.h"

namespace roo_display {

namespace {

static constexpr int kAddr1 = 0x5D;
static constexpr int kAddr2 = 0x14;

static constexpr int kTouchRead = 0x814e;
static constexpr int kPointBuffer = 0x814f;

TouchPoint ReadPoint(const roo::byte* data) {
  TouchPoint tp;
  tp.id = (uint16_t)data[0];
  tp.x = ((int16_t)data[2] << 8) | (int16_t)data[1];
  tp.y = ((int16_t)data[4] << 8) | (int16_t)data[3];
  tp.z = 1024;
  return tp;
}

}  // namespace

void TouchGt911::initTouch() {
  pinMode(pinRst_, OUTPUT);
  digitalWrite(pinRst_, 0);
  if (pinIntr_ >= 0) {
    pinMode(pinIntr_, OUTPUT);
    digitalWrite(pinIntr_, 0);
  }
  reset();
}

TouchGt911::TouchGt911(int8_t pinIntr, int8_t pinRst, long reset_low_hold_ms)
    : TouchGt911(I2cMasterBusHandle(), pinIntr, pinRst, reset_low_hold_ms) {}

TouchGt911::TouchGt911(I2cMasterBusHandle i2c, int8_t pinIntr, int8_t pinRst,
                       long reset_low_hold_ms)
    : BasicTouchDevice<5>(Config{.min_sampling_interval_ms = 20,
                                 .touch_intertia_ms = 30,
                                 .smoothing_factor = 0.0}),
      addr_(kAddr1),
      pinIntr_(pinIntr),
      pinRst_(pinRst),
      i2c_slave_(i2c, addr_),
      reset_low_hold_ms_(reset_low_hold_ms),
      ready_(false) {}

void TouchGt911::reset() {
  if (reset_thread_.joinable()) {
    return;
  }
  ready_ = false;
  // Initialize the reset asynchronously to avoid blocking the main thread.
  reset_thread_ = roo::thread([this]() {
    if (pinIntr_ >= 0) {
      digitalWrite(pinIntr_, 0);
    }
    digitalWrite(pinRst_, 0);
    delay(reset_low_hold_ms_);
    if (pinIntr_ >= 0) {
      digitalWrite(pinIntr_, addr_ == kAddr2);
      delay(1);
    }
    digitalWrite(pinRst_, 1);
    if (pinIntr_ >= 0) {
      delay(5);
      digitalWrite(pinIntr_, 0);
    }
    // Note: the programming guide for 911 says that it is at least 50ms until
    // the interrupts start working, and up to 100 ms before scan starts
    // clocking.
    delay(100);
    ready_ = true;
  });
}

int TouchGt911::readTouch(TouchPoint* points) {
  if (!ready_) return 0;
  if (reset_thread_.joinable()) {
    reset_thread_.join();
  }
  roo::byte status;
  if (!readByte(kTouchRead, status)) {
    reset();
    return 0;
  }
  bool ready = (status & roo::byte{0x80}) != roo::byte{0};
  // uint8_t have_key = (status & 0x10) != 0;
  if (!ready) return 0;
  int touches = (int)(status & roo::byte{0xF});
  roo::byte data[7];
  for (uint8_t i = 0; i < touches; i++) {
    readBlock(data, kPointBuffer + i * 8, 7);
    points[i] = ReadPoint(data);
  }
  // Clear the 'ready' flag.
  writeByte(kTouchRead, roo::byte{0});
  return touches;
}

bool TouchGt911::readByte(uint16_t reg, roo::byte& result) {
  roo::byte addr[] = {(roo::byte)(reg >> 8), (roo::byte)(reg & 0xFF)};
  if (!i2c_slave_.transmit(addr, 2)) return false;
  return (i2c_slave_.receive(&result, 1) == 1);
}

void TouchGt911::writeByte(uint16_t reg, roo::byte val) {
  roo::byte buf[] = {(roo::byte)(reg >> 8), (roo::byte)(reg & 0xFF), val};
  i2c_slave_.transmit(buf, 3);
}

void TouchGt911::readBlock(roo::byte* buf, uint16_t reg, uint8_t size) {
  roo::byte addr[] = {(roo::byte)(reg >> 8), (roo::byte)(reg & 0xFF)};
  i2c_slave_.transmit(addr, 2);
  i2c_slave_.receive(buf, size);
}

}  // namespace roo_display

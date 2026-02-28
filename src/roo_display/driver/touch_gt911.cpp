#include "roo_display/driver/touch_gt911.h"

#include "roo_display/core/device.h"
#include "roo_display/hal/gpio.h"
#include "roo_threads.h"
#include "roo_threads/thread.h"
#include "roo_time.h"

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

TouchGt911::TouchGt911(GpioSetter pinIntr, GpioSetter pinRst,
                       long reset_low_hold_ms)
    : TouchGt911(I2cMasterBusHandle(), std::move(pinIntr), std::move(pinRst),
                 reset_low_hold_ms) {}

TouchGt911::TouchGt911(I2cMasterBusHandle i2c, GpioSetter pinIntr,
                       GpioSetter pinRst, long reset_low_hold_ms)
    : BasicTouchDevice<5>(Config{.min_sampling_interval_ms = 20,
                                 .touch_intertia_ms = 30,
                                 .smoothing_factor = 0.0}),
      addr_(kAddr1),
      pinIntr_(pinIntr),
      pinRst_(pinRst),
      i2c_slave_(i2c, addr_),
      reset_low_hold_ms_(reset_low_hold_ms),
      ready_(false) {}

void TouchGt911::initTouch() {
  pinRst_.init();
  pinRst_.setLow();
  if (pinIntr_.isDefined()) {
    pinIntr_.init();
    pinIntr_.setLow();
  }
  i2c_slave_.init();
  reset();
}

void TouchGt911::reset() {
  if (reset_thread_.joinable()) {
    return;
  }
  ready_ = false;
  // Initialize the reset asynchronously to avoid blocking the main thread.
  reset_thread_ = roo::thread([this]() {
    if (pinIntr_.isDefined()) {
      pinIntr_.setLow();
    }
    pinRst_.setLow();
    roo::this_thread::sleep_for(roo_time::Millis((reset_low_hold_ms_)));
    if (pinIntr_.isDefined()) {
      if (addr_ == kAddr1) {
        pinIntr_.setLow();
      } else {
        pinIntr_.setHigh();
      }
      roo::this_thread::sleep_for(roo_time::Millis(1));
    }
    pinRst_.setHigh();
    if (pinIntr_.isDefined()) {
      roo::this_thread::sleep_for(roo_time::Millis(5));
      pinIntr_.setLow();
    }
    // Note: the programming guide for 911 says that it is at least 50ms until
    // the interrupts start working, and up to 100 ms before scan starts
    // clocking.
    roo::this_thread::sleep_for(roo_time::Millis(100));
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

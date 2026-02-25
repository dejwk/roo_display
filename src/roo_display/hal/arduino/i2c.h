#pragma once

#if defined(ARDUINO) || defined(ROO_TESTING)
#include <Arduino.h>

#include "roo_backport.h"
#include "roo_backport/byte.h"

// Generic Arduino implementation.

#include <Wire.h>

#include "roo_logging.h"

namespace roo_display {

// I2C master bus, using the default <Wire.h> from the Arduino framework.
class ArduinoI2cMasterBusHandle {
 public:
  // Creates the I2C master bus handle that references the specified Wire.
  ArduinoI2cMasterBusHandle(decltype(Wire)& wire = Wire) : wire_(wire) {}

  void init() { wire_.begin(); }

  void init(int sda, int scl, uint32_t frequency = 0) {
    wire_.begin(sda, scl, frequency);
  }

 private:
  friend class ArduinoI2cSlaveDevice;

  decltype(Wire)& wire_;
};

class ArduinoI2cSlaveDevice {
 public:
  ArduinoI2cSlaveDevice(ArduinoI2cMasterBusHandle& bus_handle,
                        uint8_t slave_address)
      : wire_(bus_handle.wire_), slave_address_(slave_address) {}

  void init() {}

  bool transmit(const roo::byte* data, size_t len) {
    wire_.beginTransmission(slave_address_);
    size_t written = wire_.write((const uint8_t*)data, len);
    if (written < len) return false;
    uint8_t result = wire_.endTransmission();
    return (result == 0);
  }

  size_t receive(roo::byte* data, size_t len) {
    size_t received = wire_.requestFrom(slave_address_, len);
    for (size_t i = 0; i < received; ++i) {
      *data++ = static_cast<roo::byte>(wire_.read());
    }
    return received;
  }

 private:
  decltype(Wire)& wire_;
  uint8_t slave_address_;
};

}  // namespace roo_display

#endif  // defined(ARDUINO) || defined(ROO_TESTING)
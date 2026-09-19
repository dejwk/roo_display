#pragma once

// Compatibility aliases. New users should include roo_io/i2c/arduino/i2c.h.
#include "roo_io/i2c/arduino/i2c.h"
#if defined(ARDUINO)
namespace roo_display {
using ArduinoI2cMasterBusHandle = roo_io::ArduinoI2cMasterBusHandle;
using ArduinoI2cSlaveDevice = roo_io::ArduinoI2cSlaveDevice;
}  // namespace roo_display
#endif

#pragma once

// Compatibility aliases. New users should include roo_io/i2c/i2c.h.
#include "roo_io/i2c/i2c.h"
#if defined(ARDUINO)
#include "roo_display/hal/arduino/i2c.h"
#elif defined(ESP_PLATFORM)
#include "roo_display/hal/esp32/i2c.h"
#endif

#if defined(ARDUINO) || defined(ESP_PLATFORM)
namespace roo_display {
using I2cMasterBusHandle = roo_io::I2cMasterBusHandle;
using I2cSlaveDevice = roo_io::I2cSlaveDevice;
}  // namespace roo_display
#endif

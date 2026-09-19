#pragma once

// Compatibility aliases. New users should include roo_io/i2c/esp32/i2c.h.
#include "roo_io/i2c/esp32/i2c.h"
#if defined(ESP_PLATFORM) && !defined(ARDUINO)
namespace roo_display {
using Esp32I2cMasterBusHandle = roo_io::Esp32I2cMasterBusHandle;
using Esp32I2cSlaveDevice = roo_io::Esp32I2cSlaveDevice;
}  // namespace roo_display
#endif

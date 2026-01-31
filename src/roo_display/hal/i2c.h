#pragma once

#include "roo_display/hal/config.h"

#if defined(ARDUINO)

#include "roo_display/hal/arduino/i2c.h"

namespace roo_display {

using I2cMasterBusHandle = ArduinoI2cMasterBusHandle;
using I2cSlaveDevice = ArduinoI2cSlaveDevice;

}  // namespace roo_display

#elif defined(ESP_PLATFORM)

#include "roo_display/hal/esp32/i2c.h"

namespace roo_display {

using I2cMasterBusHandle = Esp32I2cMasterBusHandle;
using I2cSlaveDevice = Esp32I2cSlaveDevice;

}  // namespace roo_display

#endif  // defined ARDUINO

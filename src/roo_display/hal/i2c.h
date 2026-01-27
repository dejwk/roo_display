#if (defined ARDUINO)

#include "roo_display/hal/arduino/i2c.h"

namespace roo_display {

using I2cMasterBusHandle = ArduinoI2cMasterBusHandle;
using I2cSlaveDevice = ArduinoI2cSlaveDevice;

}  // namespace roo_display

#endif  // defined ARDUINO

#pragma once

#include <Arduino.h>

#if (defined(ESP32) && (CONFIG_IDF_TARGET_ESP32))

#include "roo_display/hal/esp32/spi.h"

namespace roo_display {
using DefaultSpi = esp32::Vspi;
}

#else

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
using DefaultSpi = GenericSpi;
}

#endif

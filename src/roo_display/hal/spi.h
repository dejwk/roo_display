#pragma once

#include <Arduino.h>

#if defined(ESP32)

#if (CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32C3)

#include "roo_display/hal/esp32/spi.h"

namespace roo_display {
#if CONFIG_IDF_TARGET_ESP32
using DefaultSpi = esp32::Vspi;
#else
using DefaultSpi = esp32::Fspi;
#endif
}  // namespace roo_display

#else

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
using DefaultSpi = GenericSpi;
}

#endif

#else

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
using DefaultSpi = GenericSpi;
}

#endif

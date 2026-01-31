#pragma once

#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)

#include "roo_display/hal/esp32/spi.h"

namespace roo_display {
#if CONFIG_IDF_TARGET_ESP32
using DefaultSpi = esp32::Vspi;
#else
using DefaultSpi = esp32::Fspi;
#endif
}  // namespace roo_display

#elif defined(ARDUINO)

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
using DefaultSpi = ArduinoSpi;
}

#endif

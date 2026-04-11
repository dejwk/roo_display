#include "roo_display/hal/esp32/spi_config.h"

using namespace roo_display::esp32;

#ifndef ROO_DISPLAY_ESP32_DEFAULT_SPI_ASYNC_MODE
#define ROO_DISPLAY_ESP32_DEFAULT_SPI_ASYNC_MODE 0
#endif

ROO_FLAG(SpiAsyncMode, roo_display_esp32_spi_async,
         (SpiAsyncMode)(ROO_DISPLAY_ESP32_DEFAULT_SPI_ASYNC_MODE));

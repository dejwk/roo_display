#pragma once

#include "esp_intr_alloc.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "roo_logging.h"

#ifndef ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM
#define ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM 1
#endif

#if ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM
#define ROO_DISPLAY_SPI_ASYNC_ISR_ATTR IRAM_ATTR
#else
#define ROO_DISPLAY_SPI_ASYNC_ISR_ATTR
#endif

namespace roo_display {
namespace esp32 {

// Copies a small block of memory from src to dest using an optimized routine
// suitable for use in an ISR.
ROO_DISPLAY_SPI_ASYNC_ISR_ATTR void memcpy_isr(void* dest, const void* src,
                                               size_t n);

inline bool IsInternalMemory(const void* data) {
    return esp_ptr_internal(data);
}

}  // namespace esp32
}  // namespace roo_display

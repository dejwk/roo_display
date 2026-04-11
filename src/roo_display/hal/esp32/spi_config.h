#pragma once

#include "roo_flags.h"

namespace roo_display {
namespace esp32 {

enum SpiAsyncMode : int {
  // No-IRQ.
  kSpiAsyncModeSync = 0,
  // IRQ-based async with eager, busy-loop completion on await.
  kSpiAsyncModeIsrEager = 1,
  // IRQ-based async with no eager completion.
  kSpiAsyncModeIsrStrict = 2,
  // DMA-backed async pipeline.
  kSpiAsyncModeDmaPipeline = 3,
};

}
}  // namespace roo_display

// 0: async disabled (always synchronous).
// 1: async enabled with eager completion on await.
// 2: async enabled with no eager completion.
// 3: DMA-backed async pipeline.
ROO_DECLARE_FLAG(roo_display::esp32::SpiAsyncMode, roo_display_esp32_spi_async);

namespace roo_display {
namespace esp32 {

inline SpiAsyncMode GetSpiAsyncModeFromFlag() {
  return GET_ROO_FLAG(roo_display_esp32_spi_async);
}

}  // namespace esp32
}  // namespace roo_display

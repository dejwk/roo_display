#pragma once

#include "roo_flags.h"

// 0: async disabled (always synchronous).
// 1: async enabled with eager completion on await.
// 2: async enabled with no eager completion.
// 3: DMA-backed async pipeline.
ROO_DECLARE_FLAG(int, roo_display_esp32_spi_async);

namespace roo_display {
namespace esp32 {

enum SpiAsyncMode : int {
	kSpiAsyncModeSync = 0,
	kSpiAsyncModeIsrEager = 1,
	kSpiAsyncModeIsrStrict = 2,
	kSpiAsyncModeDmaPipeline = 3,
};

inline SpiAsyncMode GetSpiAsyncModeFromFlag() {
	int mode = GET_ROO_FLAG(roo_display_esp32_spi_async);
	if (mode <= 0) return kSpiAsyncModeSync;
	if (mode >= 3) return kSpiAsyncModeDmaPipeline;
	return static_cast<SpiAsyncMode>(mode);
}

}  // namespace esp32
}  // namespace roo_display

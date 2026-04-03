#pragma once

#include "roo_flags.h"

// 0: async disabled (always synchronous).
// 1: async enabled with eager completion on await.
// 2: async enabled with no eager completion.
// 3: DMA-backed async pipeline.
ROO_DECLARE_FLAG(int, roo_display_esp32_spi_async);

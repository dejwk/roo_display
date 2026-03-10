#pragma once

#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)

#include "roo_display/hal/esp32/async_blit.h"

#else

#include "roo_display/hal/default/async_blit.h"

#endif

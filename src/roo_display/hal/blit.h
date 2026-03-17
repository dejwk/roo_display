#pragma once

#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)

#include "roo_display/hal/esp32/blit.h"

#else

#include "roo_display/hal/default/blit.h"

#endif

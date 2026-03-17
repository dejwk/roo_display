#pragma once

#include <stddef.h>

#include "roo_backport/byte.h"

namespace roo_display {

void blit_init();

void blit_deinit();

void blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
          size_t dst_stride, size_t width, size_t height);

}  // namespace roo_display

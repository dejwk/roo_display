#pragma once

#include <stddef.h>

#include "roo_backport/byte.h"

namespace roo_display {

void async_blit_init();

void async_blit_deinit();

void async_blit_await();

void async_blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
                size_t dst_stride, size_t width, size_t height);

}  // namespace roo_display

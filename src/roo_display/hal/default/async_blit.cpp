#include "roo_display/hal/config.h"

#if !defined(ESP_PLATFORM)

#include <cstring>

#include "roo_display/hal/default/async_blit.h"

namespace roo_display {

void async_blit_init() {}

void async_blit_deinit() {}

void async_blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
                size_t dst_stride, size_t width, size_t height,
                std::function<void()> cb) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
    if (cb) cb();
    return;
  }

  if (src_stride == width && dst_stride == width) {
    std::memcpy(dst_ptr, src_ptr, width * height);
  } else {
    const roo::byte* src_row = src_ptr;
    roo::byte* dst_row = dst_ptr;
    for (size_t row = 0; row < height; ++row) {
      std::memcpy(dst_row, src_row, width);
      src_row += src_stride;
      dst_row += dst_stride;
    }
  }

  if (cb) cb();
}

}  // namespace roo_display

#endif  // !defined(ESP_PLATFORM)

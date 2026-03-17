#include "roo_display/hal/config.h"

#if !defined(ESP_PLATFORM)

#include <cstring>

#include "roo_display/hal/default/blit.h"

namespace roo_display {

void blit_init() {}

void blit_deinit() {}

void blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
          size_t dst_stride, size_t width, size_t height) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
    return;
  }

  if (src_stride == width && dst_stride == width) {
    std::memcpy(dst_ptr, src_ptr, width * height);
    return;
  }

  const roo::byte* src_row = src_ptr;
  roo::byte* dst_row = dst_ptr;
  for (size_t row = 0; row < height; ++row) {
    std::memcpy(dst_row, src_row, width);
    src_row += src_stride;
    dst_row += dst_stride;
  }
}

}  // namespace roo_display

#endif  // !defined(ESP_PLATFORM)

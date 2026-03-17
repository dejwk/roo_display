#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "roo_display/hal/esp32/blit.h"

#if CONFIG_IDF_TARGET_ESP32S3

#include "esp_async_memcpy.h"
#include "esp_err.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "roo_logging.h"

#ifndef ROO_DISPLAY_DMA_MIN_BYTES
#define ROO_DISPLAY_DMA_MIN_BYTES 256
#endif

namespace roo_display {
namespace {

constexpr size_t kDmaAlign = 16;
constexpr size_t kExtDmaAlign = 32;
constexpr size_t kDmaThresholdBytes = ROO_DISPLAY_DMA_MIN_BYTES;

struct BlitState {
  BlitState()
      : done(xSemaphoreCreateBinaryStatic(&done_buf)),
        lock(xSemaphoreCreateMutexStatic(&lock_buf)) {}

  async_memcpy_handle_t handle = nullptr;
  StaticSemaphore_t done_buf;
  SemaphoreHandle_t done = nullptr;
  StaticSemaphore_t lock_buf;
  SemaphoreHandle_t lock = nullptr;

  const roo::byte* src_row = nullptr;
  roo::byte* dst_row = nullptr;
  size_t src_stride = 0;
  size_t dst_stride = 0;
  size_t row_bytes = 0;
  size_t remaining_rows = 0;
  bool transfer_failed = false;
  bool install_attempted = false;
};

BlitState& state() {
  static BlitState s;
  return s;
}

void copy_sync(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
               size_t dst_stride, size_t width, size_t height) {
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

inline bool is_aligned(const void* ptr, size_t align) {
  return (reinterpret_cast<uintptr_t>(ptr) % align) == 0;
}

inline size_t dma_align_for_ptr(const void* ptr) {
  return esp_ptr_external_ram(ptr) ? kExtDmaAlign : kDmaAlign;
}

inline bool can_dma_copy_rows(const void* src, const void* dst,
                              size_t row_bytes, size_t src_stride,
                              size_t dst_stride) {
  const bool src_dma_capable =
      esp_ptr_dma_capable(src) || esp_ptr_dma_ext_capable(src);
  const bool dst_dma_capable =
      esp_ptr_dma_capable(dst) || esp_ptr_dma_ext_capable(dst);
  const size_t required_align =
      std::max(dma_align_for_ptr(src), dma_align_for_ptr(dst));
  return row_bytes > 0 && is_aligned(src, required_align) &&
         is_aligned(dst, required_align) && (row_bytes % required_align) == 0 &&
         (src_stride % required_align) == 0 &&
         (dst_stride % required_align) == 0 && src_dma_capable &&
         dst_dma_capable;
}

bool on_copy_done(async_memcpy_handle_t handle, async_memcpy_event_t*,
                  void* ctx) {
  auto* st = static_cast<BlitState*>(ctx);
  if (st->remaining_rows > 0) {
    st->src_row += st->src_stride;
    st->dst_row += st->dst_stride;
    --st->remaining_rows;
  }

  if (st->remaining_rows == 0 || st->transfer_failed) {
    BaseType_t high_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(st->done, &high_wakeup);
    return high_wakeup == pdTRUE;
  }

  esp_err_t err =
      esp_async_memcpy(handle, st->dst_row, const_cast<roo::byte*>(st->src_row),
                       st->row_bytes, on_copy_done, st);
  if (err != ESP_OK) {
    st->transfer_failed = true;
    BaseType_t high_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(st->done, &high_wakeup);
    return high_wakeup == pdTRUE;
  }
  return false;
}

bool dma_copy_rows_and_wait(BlitState& st, const roo::byte* src, roo::byte* dst,
                            size_t src_stride, size_t dst_stride,
                            size_t row_bytes, size_t row_count) {
  while (xSemaphoreTake(st.done, 0) == pdTRUE) {
  }

  st.src_row = src;
  st.dst_row = dst;
  st.src_stride = src_stride;
  st.dst_stride = dst_stride;
  st.row_bytes = row_bytes;
  st.remaining_rows = row_count;
  st.transfer_failed = false;

  esp_err_t err = esp_async_memcpy(st.handle, st.dst_row,
                                   const_cast<roo::byte*>(st.src_row),
                                   st.row_bytes, on_copy_done, &st);
  CHECK_EQ(err, ESP_OK) << esp_err_to_name(err);

  CHECK_EQ(xSemaphoreTake(st.done, portMAX_DELAY), pdTRUE);
  return !st.transfer_failed;
}

void ensure_init() {
  BlitState& st = state();
  if (st.handle != nullptr || st.install_attempted) return;
  CHECK_NOTNULL(st.done);
  CHECK_NOTNULL(st.lock);

  st.install_attempted = true;
  async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
  cfg.backlog = 2;
  async_memcpy_handle_t handle = nullptr;
  if (esp_async_memcpy_install(&cfg, &handle) == ESP_OK) {
    st.handle = handle;
  }
}

}  // namespace

void blit_init() { ensure_init(); }

void blit_deinit() {
  BlitState& st = state();
  if (st.handle == nullptr || st.lock == nullptr) return;
  if (xSemaphoreTake(st.lock, 0) != pdTRUE) return;
  esp_async_memcpy_uninstall(st.handle);
  st.handle = nullptr;
  st.install_attempted = false;
  xSemaphoreGive(st.lock);
}

void blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
          size_t dst_stride, size_t width, size_t height) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
    return;
  }

  // Normalize contiguous transfer into one super-row so threshold and DMA
  // eligibility checks can share one code path.
  size_t dma_row_bytes = width;
  size_t dma_row_count = height;
  size_t dma_src_stride = src_stride;
  size_t dma_dst_stride = dst_stride;
  if (src_stride == width && dst_stride == width) {
    dma_row_bytes = width * height;
    dma_row_count = 1;
    dma_src_stride = dma_row_bytes;
    dma_dst_stride = dma_row_bytes;
  }

  if (dma_row_bytes < kDmaThresholdBytes ||
      !can_dma_copy_rows(src_ptr, dst_ptr, dma_row_bytes, dma_src_stride,
                         dma_dst_stride)) {
    copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
    return;
  }

  ensure_init();
  BlitState& st = state();
  CHECK_EQ(xSemaphoreTake(st.lock, portMAX_DELAY), pdTRUE);

  bool success =
      dma_copy_rows_and_wait(st, src_ptr, dst_ptr, dma_src_stride,
                             dma_dst_stride, dma_row_bytes, dma_row_count);

  xSemaphoreGive(st.lock);

  if (!success) {
    copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
  }
}

}  // namespace roo_display

#else

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

#endif

#endif  // defined(ESP_PLATFORM)

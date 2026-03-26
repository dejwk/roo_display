#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "roo_display/hal/esp32/async_blit.h"

#if CONFIG_IDF_TARGET_ESP32S3

#include "esp_async_memcpy.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "roo_logging.h"

namespace roo_display {
namespace {

constexpr size_t kDmaAlign = 16;
constexpr size_t kExtDmaAlign = 32;
constexpr size_t kMinRowDmaBytes = 128;
constexpr uint32_t kStatsLogInterval = 120;
constexpr const char* kTag = "async_blit";

struct AsyncBlitStats {
  uint32_t total_requests = 0;
  uint32_t dma_contiguous = 0;
  uint32_t dma_contiguous_split = 0;
  uint32_t dma_rowwise = 0;
  uint32_t dma_rowwise_split = 0;
  uint32_t fallback_no_handle_or_busy = 0;
  uint32_t fallback_small_contig_no_middle = 0;
  uint32_t fallback_small_row_below_min_dma = 0;
  uint32_t fallback_small_row_no_middle = 0;
  uint32_t fallback_alignment = 0;
  uint32_t fallback_esp_err = 0;
};

struct AsyncBlitState {
  async_memcpy_handle_t handle = nullptr;
  const roo::byte* src = nullptr;
  roo::byte* dst = nullptr;
  size_t src_stride = 0;
  size_t dst_stride = 0;
  size_t row_bytes = 0;
  size_t remaining_rows = 0;
  TaskHandle_t caller_task = nullptr;
  AsyncBlitStats stats;

  // Completes the current operation from ISR context.
  //
  // Ownership model: caller_task is non-null iff an operation is in flight.
  // The ISR captures caller_task, clears it, and then notifies the captured
  // task. With this ordering, async_blit_await() is safe from lost wakeups:
  // - If await sees caller_task == nullptr, completion already happened.
  // - If await blocks, the notification is pending or will be sent by ISR.
  // This also relies on pointer-sized loads/stores being atomic on ESP32 for
  // caller_task handoff between task and ISR contexts.
  //
  // Note: because task notifications are counting, a completion token can be
  // left pending if await exits via the fast path. We explicitly drain any
  // stale token before starting a new operation in async_blit().
  void IRAM_ATTR notifyDoneFromISR() {
    TaskHandle_t caller = caller_task;
    caller_task = nullptr;
    if (caller != nullptr) {
      BaseType_t high_wakeup = pdFALSE;
      vTaskNotifyGiveFromISR(caller, &high_wakeup);
      if (high_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
      }
    }
  }
};

AsyncBlitState& state() {
  static AsyncBlitState s;
  return s;
}

void maybe_log_stats(AsyncBlitState& st) {
  if (st.stats.total_requests % kStatsLogInterval != 0) return;
  ESP_LOGI(kTag,
           "req=%u dma_contig=%u dma_contig_split=%u dma_rows=%u "
           "dma_rows_split=%u fb_busy=%u fb_small_contig_nomid=%u "
           "fb_small_row_min=%u fb_small_row_nomid=%u fb_align=%u fb_err=%u",
           st.stats.total_requests, st.stats.dma_contiguous,
           st.stats.dma_contiguous_split, st.stats.dma_rowwise,
           st.stats.dma_rowwise_split, st.stats.fallback_no_handle_or_busy,
           st.stats.fallback_small_contig_no_middle,
           st.stats.fallback_small_row_below_min_dma,
           st.stats.fallback_small_row_no_middle, st.stats.fallback_alignment,
           st.stats.fallback_esp_err);
}

void IRAM_ATTR copy_remaining_rows_sync(AsyncBlitState& st) {
  while (st.remaining_rows > 0) {
    std::memcpy(st.dst, st.src, st.row_bytes);
    st.dst += st.dst_stride;
    st.src += st.src_stride;
    --st.remaining_rows;
  }
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

inline bool can_dma_transfer(const void* src, const void* dst, size_t n) {
  const bool src_dma_capable =
      esp_ptr_dma_capable(src) || esp_ptr_dma_ext_capable(src);
  const bool dst_dma_capable =
      esp_ptr_dma_capable(dst) || esp_ptr_dma_ext_capable(dst);
  const size_t required_align =
      std::max(dma_align_for_ptr(src), dma_align_for_ptr(dst));
  return n > 0 && is_aligned(src, required_align) &&
         is_aligned(dst, required_align) && (n % required_align) == 0 &&
         src_dma_capable && dst_dma_capable;
}

void log_dma_reject(const char* mode, const void* src, const void* dst,
                    size_t bytes, size_t src_stride, size_t dst_stride,
                    size_t height) {
  const bool src_dma_capable =
      esp_ptr_dma_capable(src) || esp_ptr_dma_ext_capable(src);
  const bool dst_dma_capable =
      esp_ptr_dma_capable(dst) || esp_ptr_dma_ext_capable(dst);
  const size_t required_align =
      std::max(dma_align_for_ptr(src), dma_align_for_ptr(dst));
  ESP_LOGW(
      kTag,
      "reject %s src=%p dst=%p bytes=%u h=%u req_align=%u src_mod16=%u "
      "dst_mod16=%u bytes_mod16=%u src_mod32=%u dst_mod32=%u bytes_mod32=%u "
      "src_stride=%u dst_stride=%u src_stride_mod16=%u dst_stride_mod16=%u "
      "src_dma=%u "
      "dst_dma=%u",
      mode, src, dst, static_cast<unsigned>(bytes),
      static_cast<unsigned>(height), static_cast<unsigned>(required_align),
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(src) % kDmaAlign),
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(dst) % kDmaAlign),
      static_cast<unsigned>(bytes % kDmaAlign),
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(src) % kExtDmaAlign),
      static_cast<unsigned>(reinterpret_cast<uintptr_t>(dst) % kExtDmaAlign),
      static_cast<unsigned>(bytes % kExtDmaAlign),
      static_cast<unsigned>(src_stride), static_cast<unsigned>(dst_stride),
      static_cast<unsigned>(src_stride % kDmaAlign),
      static_cast<unsigned>(dst_stride % kDmaAlign),
      static_cast<unsigned>(src_dma_capable),
      static_cast<unsigned>(dst_dma_capable));
}

inline bool are_strides_aligned(size_t src_stride, size_t dst_stride,
                                const void* src, const void* dst) {
  const size_t required_align =
      std::max(dma_align_for_ptr(src), dma_align_for_ptr(dst));
  return (src_stride % required_align) == 0 &&
         (dst_stride % required_align) == 0;
}

inline bool can_dma_rowwise_transfer(const roo::byte* src, roo::byte* dst,
                                     size_t row_bytes, size_t src_stride,
                                     size_t dst_stride) {
  return can_dma_transfer(src, dst, row_bytes) &&
         are_strides_aligned(src_stride, dst_stride, src, dst);
}

inline size_t ptr_mod(const void* ptr, size_t align) {
  return reinterpret_cast<uintptr_t>(ptr) % align;
}

bool split_for_aligned_middle(const roo::byte* src_ptr, roo::byte* dst_ptr,
                              size_t bytes, size_t align, size_t& left,
                              size_t& middle, size_t& right) {
  if (bytes == 0 || align == 0) return false;
  if (ptr_mod(src_ptr, align) != ptr_mod(dst_ptr, align)) return false;

  left = (align - ptr_mod(src_ptr, align)) % align;
  if (left > bytes) return false;
  right = (bytes - left) % align;
  middle = bytes - left - right;
  return middle > 0;
}

bool IRAM_ATTR on_copy_done(async_memcpy_handle_t, async_memcpy_event_t*,
                            void* cb_args) {
  auto* st = static_cast<AsyncBlitState*>(cb_args);
  if (st == nullptr) return false;

  if (st->remaining_rows == 0) {
    st->notifyDoneFromISR();
    return false;
  }

  st->src += st->src_stride;
  st->dst += st->dst_stride;
  --st->remaining_rows;

  if (st->remaining_rows == 0) {
    st->notifyDoneFromISR();
    return false;
  }

  esp_err_t err =
      esp_async_memcpy(st->handle, st->dst, const_cast<roo::byte*>(st->src),
                       st->row_bytes, on_copy_done, st);
  if (err != ESP_OK) {
    copy_remaining_rows_sync(*st);
    st->notifyDoneFromISR();
  }
  return false;
}

bool start_async_copy(AsyncBlitState& st, const roo::byte* src, roo::byte* dst,
                      size_t src_stride, size_t dst_stride, size_t row_bytes,
                      size_t remaining_rows, const roo::byte* fallback_src,
                      size_t fallback_src_stride, roo::byte* fallback_dst,
                      size_t fallback_dst_stride, size_t fallback_width,
                      size_t fallback_height) {
  st.src = src;
  st.dst = dst;
  st.src_stride = src_stride;
  st.dst_stride = dst_stride;
  st.row_bytes = row_bytes;
  st.remaining_rows = remaining_rows;
  st.caller_task = xTaskGetCurrentTaskHandle();

  esp_err_t err =
      esp_async_memcpy(st.handle, st.dst, const_cast<roo::byte*>(st.src),
                       st.row_bytes, on_copy_done, &st);
  if (err == ESP_OK) return true;

  ++st.stats.fallback_esp_err;
  st.caller_task = nullptr;
  copy_sync(fallback_src, fallback_src_stride, fallback_dst,
            fallback_dst_stride, fallback_width, fallback_height);
  return false;
}

}  // namespace

void async_blit_init() {
  AsyncBlitState& st = state();
  if (st.handle != nullptr) return;

  async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
  cfg.backlog = 2;
  async_memcpy_handle_t handle = nullptr;
  if (esp_async_memcpy_install(&cfg, &handle) == ESP_OK) {
    st.handle = handle;
  }
}

void async_blit_deinit() {
  AsyncBlitState& st = state();
  if (st.handle != nullptr && st.caller_task == nullptr) {
    esp_async_memcpy_uninstall(st.handle);
    st.handle = nullptr;
  }
}

void async_blit_await() {
  AsyncBlitState& st = state();
  TaskHandle_t caller = st.caller_task;
  if (caller == nullptr) return;

  CHECK_EQ(caller, xTaskGetCurrentTaskHandle())
      << "async_blit_await() must be called by the same task as async_blit()";

  do {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  } while (st.caller_task != nullptr);
}

void async_blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
                size_t dst_stride, size_t width, size_t height) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
    return;
  }

  AsyncBlitState& st = state();
  CHECK(st.caller_task == nullptr)
      << "previous async_blit() still in progress; call async_blit_await()";

  // Drain any stale completion token left from a previous operation.
  // This keeps notification state aligned 1:1 with newly started transfers.
  (void)ulTaskNotifyTake(pdTRUE, 0);

  ++st.stats.total_requests;
  maybe_log_stats(st);
  const bool contiguous = (src_stride == width && dst_stride == width);
  if (st.handle == nullptr) {
    ++st.stats.fallback_no_handle_or_busy;
    copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
    return;
  }

  if (contiguous) {
    const size_t total_bytes = width * height;
    if (!can_dma_transfer(src_ptr, dst_ptr, total_bytes)) {
      size_t left = 0;
      size_t middle = 0;
      size_t right = 0;
      const bool same_ext_mod =
          ptr_mod(src_ptr, kExtDmaAlign) == ptr_mod(dst_ptr, kExtDmaAlign);
      const bool can_split = split_for_aligned_middle(
          src_ptr, dst_ptr, total_bytes, kExtDmaAlign, left, middle, right);
      const roo::byte* src_mid = src_ptr + left;
      roo::byte* dst_mid = dst_ptr + left;
      if (can_split && can_dma_transfer(src_mid, dst_mid, middle)) {
        if (left > 0) {
          std::memcpy(dst_ptr, src_ptr, left);
        }
        if (right > 0) {
          std::memcpy(dst_ptr + left + middle, src_ptr + left + middle, right);
        }

        ++st.stats.dma_contiguous_split;
        start_async_copy(st, src_mid, dst_mid, 0, 0, middle, 0, src_ptr,
                         src_stride, dst_ptr, dst_stride, width, height);
        return;
      }

      // For small total sizes there may be no aligned middle chunk to DMA even
      // though src/dst share the same ext-DMA modulo. This is expected.
      if (same_ext_mod && !can_split) {
        ++st.stats.fallback_small_contig_no_middle;
        copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
        return;
      }

      ++st.stats.fallback_alignment;
      log_dma_reject("contig", src_ptr, dst_ptr, total_bytes, src_stride,
                     dst_stride, height);
      copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      return;
    }
  } else {
    if (width < kMinRowDmaBytes) {
      ++st.stats.fallback_small_row_below_min_dma;
      copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      return;
    }

    const bool rowwise_direct_dma_ok = can_dma_rowwise_transfer(
        src_ptr, dst_ptr, width, src_stride, dst_stride);
    if (!rowwise_direct_dma_ok) {
      size_t left = 0;
      size_t middle = 0;
      size_t right = 0;
      const bool same_ext_mod =
          ptr_mod(src_ptr, kExtDmaAlign) == ptr_mod(dst_ptr, kExtDmaAlign);
      const bool can_split = split_for_aligned_middle(
          src_ptr, dst_ptr, width, kExtDmaAlign, left, middle, right);
      const roo::byte* src_mid = src_ptr + left;
      roo::byte* dst_mid = dst_ptr + left;
      if (can_split && can_dma_rowwise_transfer(src_mid, dst_mid, middle,
                                                src_stride, dst_stride)) {
        if (left > 0) {
          copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, left, height);
        }
        if (right > 0) {
          copy_sync(src_ptr + left + middle, src_stride,
                    dst_ptr + left + middle, dst_stride, right, height);
        }

        ++st.stats.dma_rowwise_split;
        start_async_copy(st, src_mid, dst_mid, src_stride, dst_stride, middle,
                         height, src_ptr, src_stride, dst_ptr, dst_stride,
                         width, height);
        return;
      }

      // For small widths there may be no aligned middle chunk to DMA even
      // though src/dst share the same ext-DMA modulo. This is expected.
      if (same_ext_mod && !can_split) {
        ++st.stats.fallback_small_row_no_middle;
        copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
        return;
      }

      ++st.stats.fallback_alignment;
      log_dma_reject("row", src_ptr, dst_ptr, width, src_stride, dst_stride,
                     height);
      copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      return;
    }
  }
  if (contiguous) {
    ++st.stats.dma_contiguous;
    start_async_copy(st, src_ptr, dst_ptr, 0, 0, width * height, 0, src_ptr,
                     src_stride, dst_ptr, dst_stride, width, height);
  } else {
    ++st.stats.dma_rowwise;
    start_async_copy(st, src_ptr, dst_ptr, src_stride, dst_stride, width,
                     height, src_ptr, src_stride, dst_ptr, dst_stride, width,
                     height);
  }
}

}  // namespace roo_display

#else

namespace roo_display {

void async_blit_init() {}

void async_blit_deinit() {}

void async_blit_await() {}

void async_blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
                size_t dst_stride, size_t width, size_t height) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
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
}

}  // namespace roo_display

#endif

#endif  // defined(ESP_PLATFORM)

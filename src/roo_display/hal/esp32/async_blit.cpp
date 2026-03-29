#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

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
constexpr size_t kQueuedBlits = 32;
constexpr uint32_t kStatsLogInterval = 120;
constexpr const char* kTag = "async_blit";

struct DmaBlitOp {
  const roo::byte* src = nullptr;
  roo::byte* dst = nullptr;
  size_t src_stride = 0;
  size_t dst_stride = 0;
  size_t row_bytes = 0;
  size_t remaining_rows = 0;
};

class DmaBlitQueue {
 public:
  DmaBlitQueue() : buf_(std::make_unique<DmaBlitOp[]>(kQueuedBlits)) {}

  DmaBlitQueue(const DmaBlitQueue&) = delete;
  DmaBlitQueue& operator=(const DmaBlitQueue&) = delete;

  bool empty() const { return used_ == 0; }
  bool full() const { return used_ == kQueuedBlits; }

  bool push(DmaBlitOp op) {
    if (full()) return false;
    size_t pos = head_ + used_;
    if (pos >= kQueuedBlits) pos -= kQueuedBlits;
    buf_[pos] = std::move(op);
    ++used_;
    return true;
  }

  bool pop(DmaBlitOp& op) {
    if (empty()) return false;
    op = std::move(buf_[head_]);
    ++head_;
    if (head_ == kQueuedBlits) head_ = 0;
    --used_;
    return true;
  }

  size_t size() const { return used_; }

 private:
  std::unique_ptr<DmaBlitOp[]> buf_;
  size_t head_ = 0;
  size_t used_ = 0;
};

struct AsyncBlitStats {
  uint32_t total_requests = 0;
  uint32_t queue_len_max = 0;
  uint32_t queue_full_blocks = 0;
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
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  const roo::byte* src = nullptr;
  roo::byte* dst = nullptr;
  size_t src_stride = 0;
  size_t dst_stride = 0;
  size_t row_bytes = 0;
  size_t remaining_rows = 0;
  bool dma_active = false;
  DmaBlitQueue queue;
  TaskHandle_t owner_task = nullptr;
  AsyncBlitStats stats;

  // Completes the current operation from ISR context.
  //
  // Notification model: completion of each queued DMA blit notifies owner_task.
  // async_blit() uses this signal to wait for queue space. async_blit_await()
  // waits until both queue and in-flight operation are fully drained.
  void IRAM_ATTR notifyOwnerFromISR() {
    TaskHandle_t owner = owner_task;
    if (owner != nullptr) {
      BaseType_t high_wakeup = pdFALSE;
      vTaskNotifyGiveFromISR(owner, &high_wakeup);
      if (high_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
      }
    }
  }
};

AsyncBlitState& State() {
  static AsyncBlitState s;
  return s;
}

void MaybeLogStats(AsyncBlitState& st) {
  if (st.stats.total_requests % kStatsLogInterval != 0) return;
  ESP_LOGI(kTag,
           "req=%u qmax=%u qfull_wait=%u dma_contig=%u dma_contig_split=%u "
           "dma_rows=%u "
           "dma_rows_split=%u fb_busy=%u fb_small_contig_nomid=%u "
           "fb_small_row_min=%u fb_small_row_nomid=%u fb_align=%u fb_err=%u",
           st.stats.total_requests, st.stats.queue_len_max,
           st.stats.queue_full_blocks, st.stats.dma_contiguous,
           st.stats.dma_contiguous_split, st.stats.dma_rowwise,
           st.stats.dma_rowwise_split, st.stats.fallback_no_handle_or_busy,
           st.stats.fallback_small_contig_no_middle,
           st.stats.fallback_small_row_below_min_dma,
           st.stats.fallback_small_row_no_middle, st.stats.fallback_alignment,
           st.stats.fallback_esp_err);
}

void IRAM_ATTR CopyRemainingRowsSync(AsyncBlitState& st) {
  while (st.remaining_rows > 0) {
    std::memcpy(st.dst, st.src, st.row_bytes);
    st.dst += st.dst_stride;
    st.src += st.src_stride;
    --st.remaining_rows;
  }
}

void IRAM_ATTR CopyOpSync(const DmaBlitOp& op) {
  if (op.remaining_rows == 0) {
    std::memcpy(op.dst, op.src, op.row_bytes);
    return;
  }

  const roo::byte* src = op.src;
  roo::byte* dst = op.dst;
  size_t rows = op.remaining_rows;
  while (rows > 0) {
    std::memcpy(dst, src, op.row_bytes);
    src += op.src_stride;
    dst += op.dst_stride;
    --rows;
  }
}

void CopySync(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
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

inline bool IsAligned(const void* ptr, size_t align) {
  return (reinterpret_cast<uintptr_t>(ptr) % align) == 0;
}

inline size_t DmaAlignForPtr(const void* ptr) {
  return esp_ptr_external_ram(ptr) ? kExtDmaAlign : kDmaAlign;
}

inline bool CanDmaTransfer(const void* src, const void* dst, size_t n) {
  const bool src_dma_capable =
      esp_ptr_dma_capable(src) || esp_ptr_dma_ext_capable(src);
  const bool dst_dma_capable =
      esp_ptr_dma_capable(dst) || esp_ptr_dma_ext_capable(dst);
  const size_t required_align =
      std::max(DmaAlignForPtr(src), DmaAlignForPtr(dst));
  return n > 0 && IsAligned(src, required_align) &&
         IsAligned(dst, required_align) && (n % required_align) == 0 &&
         src_dma_capable && dst_dma_capable;
}

void LogDmaReject(const char* mode, const void* src, const void* dst,
                  size_t bytes, size_t src_stride, size_t dst_stride,
                  size_t height) {
  const bool src_dma_capable =
      esp_ptr_dma_capable(src) || esp_ptr_dma_ext_capable(src);
  const bool dst_dma_capable =
      esp_ptr_dma_capable(dst) || esp_ptr_dma_ext_capable(dst);
  const size_t required_align =
      std::max(DmaAlignForPtr(src), DmaAlignForPtr(dst));
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

inline bool AreStridesAligned(size_t src_stride, size_t dst_stride,
                              const void* src, const void* dst) {
  const size_t required_align =
      std::max(DmaAlignForPtr(src), DmaAlignForPtr(dst));
  return (src_stride % required_align) == 0 &&
         (dst_stride % required_align) == 0;
}

inline bool CanDmaRowwiseTransfer(const roo::byte* src, roo::byte* dst,
                                  size_t row_bytes, size_t src_stride,
                                  size_t dst_stride) {
  return CanDmaTransfer(src, dst, row_bytes) &&
         AreStridesAligned(src_stride, dst_stride, src, dst);
}

inline size_t PtrMod(const void* ptr, size_t align) {
  return reinterpret_cast<uintptr_t>(ptr) % align;
}

bool SplitForAlignedMiddle(const roo::byte* src_ptr, roo::byte* dst_ptr,
                           size_t bytes, size_t align, size_t& left,
                           size_t& middle, size_t& right) {
  if (bytes == 0 || align == 0) return false;
  if (PtrMod(src_ptr, align) != PtrMod(dst_ptr, align)) return false;

  left = (align - PtrMod(src_ptr, align)) % align;
  if (left > bytes) return false;
  right = (bytes - left) % align;
  middle = bytes - left - right;
  return middle > 0;
}

bool IRAM_ATTR OnCopyDone(async_memcpy_handle_t, async_memcpy_event_t*,
                          void* cb_args);

void SetActiveFromOp(AsyncBlitState& st, const DmaBlitOp& op) {
  st.src = op.src;
  st.dst = op.dst;
  st.src_stride = op.src_stride;
  st.dst_stride = op.dst_stride;
  st.row_bytes = op.row_bytes;
  st.remaining_rows = op.remaining_rows;
  st.dma_active = true;
}

bool StartActiveDma(AsyncBlitState& st) {
  return esp_async_memcpy(st.handle, st.dst, const_cast<roo::byte*>(st.src),
                          st.row_bytes, OnCopyDone, &st) == ESP_OK;
}

bool IRAM_ATTR CompleteCurrentAndContinueFromIsr(AsyncBlitState& st) {
  // Hand completion to the owner task first, then keep draining queued ops
  // from ISR context so the DMA engine stays busy without task intervention.
  st.notifyOwnerFromISR();
  while (true) {
    DmaBlitOp next;
    taskENTER_CRITICAL_ISR(&st.mux);
    bool has_next = st.queue.pop(next);
    if (!has_next) {
      st.dma_active = false;
      taskEXIT_CRITICAL_ISR(&st.mux);
      return false;
    }
    SetActiveFromOp(st, next);
    taskEXIT_CRITICAL_ISR(&st.mux);

    if (StartActiveDma(st)) return false;

    ++st.stats.fallback_esp_err;
    CopyOpSync(next);
    st.notifyOwnerFromISR();
  }
}

void MaybeStartNextFromTask(AsyncBlitState& st) {
  while (true) {
    DmaBlitOp next;
    taskENTER_CRITICAL(&st.mux);
    // Task-side kick: only steal work when DMA is idle; otherwise ISR will
    // continue from the active operation callback.
    if (st.dma_active || !st.queue.pop(next)) {
      taskEXIT_CRITICAL(&st.mux);
      return;
    }
    SetActiveFromOp(st, next);
    taskEXIT_CRITICAL(&st.mux);

    if (StartActiveDma(st)) return;

    ++st.stats.fallback_esp_err;
    CopyOpSync(next);
    taskENTER_CRITICAL(&st.mux);
    st.dma_active = false;
    taskEXIT_CRITICAL(&st.mux);
  }
}

void EnqueueDmaOp(AsyncBlitState& st, const DmaBlitOp& op) {
  while (true) {
    bool should_kick = false;
    taskENTER_CRITICAL(&st.mux);
    if (st.queue.push(op)) {
      if (st.queue.size() > st.stats.queue_len_max) {
        st.stats.queue_len_max = st.queue.size();
      }
      should_kick = !st.dma_active;
      taskEXIT_CRITICAL(&st.mux);
      if (should_kick) {
        MaybeStartNextFromTask(st);
      }
      return;
    }
    taskEXIT_CRITICAL(&st.mux);
    // Fixed-size queue backpressure: block producer until ISR/task completion
    // notification indicates that at least one slot may have become available.
    ++st.stats.queue_full_blocks;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

bool IRAM_ATTR OnCopyDone(async_memcpy_handle_t, async_memcpy_event_t*,
                          void* cb_args) {
  auto* st = static_cast<AsyncBlitState*>(cb_args);
  if (st == nullptr) return false;

  if (st->remaining_rows == 0) {
    return CompleteCurrentAndContinueFromIsr(*st);
  }

  st->src += st->src_stride;
  st->dst += st->dst_stride;
  --st->remaining_rows;

  if (st->remaining_rows == 0) {
    return CompleteCurrentAndContinueFromIsr(*st);
  }

  esp_err_t err =
      esp_async_memcpy(st->handle, st->dst, const_cast<roo::byte*>(st->src),
                       st->row_bytes, OnCopyDone, st);
  if (err != ESP_OK) {
    // If row chaining fails mid-op, finish the current op synchronously and
    // then transfer control to the common ISR completion path.
    CopyRemainingRowsSync(*st);
    return CompleteCurrentAndContinueFromIsr(*st);
  }
  return false;
}

}  // namespace

void async_blit_init() {
  AsyncBlitState& st = State();
  if (st.handle != nullptr) return;

  async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
  cfg.backlog = 2;
  async_memcpy_handle_t handle = nullptr;
  if (esp_async_memcpy_install(&cfg, &handle) == ESP_OK) {
    st.handle = handle;
  }
}

void async_blit_deinit() {
  AsyncBlitState& st = State();
  bool can_deinit = false;
  taskENTER_CRITICAL(&st.mux);
  can_deinit = !st.dma_active && st.queue.empty();
  taskEXIT_CRITICAL(&st.mux);
  if (st.handle != nullptr && can_deinit) {
    esp_async_memcpy_uninstall(st.handle);
    st.handle = nullptr;
    st.owner_task = nullptr;
  }
}

void async_blit_await() {
  AsyncBlitState& st = State();
  TaskHandle_t caller = xTaskGetCurrentTaskHandle();
  if (st.owner_task == nullptr) return;
  CHECK_EQ(st.owner_task, caller)
      << "async_blit_await() must be called by the same task as async_blit()";

  while (true) {
    bool idle = false;
    taskENTER_CRITICAL(&st.mux);
    idle = !st.dma_active && st.queue.empty();
    taskEXIT_CRITICAL(&st.mux);
    if (idle) return;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

void async_blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
                size_t dst_stride, size_t width, size_t height) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
    return;
  }

  AsyncBlitState& st = State();
  TaskHandle_t caller = xTaskGetCurrentTaskHandle();
  if (st.owner_task == nullptr) {
    st.owner_task = caller;
  } else {
    CHECK_EQ(st.owner_task, caller)
        << "async_blit() must be called by the same task as async_blit_await()";
  }

  ++st.stats.total_requests;
  MaybeLogStats(st);
  const bool contiguous = (src_stride == width && dst_stride == width);
  if (st.handle == nullptr) {
    ++st.stats.fallback_no_handle_or_busy;
    CopySync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
    return;
  }

  if (contiguous) {
    const size_t total_bytes = width * height;
    if (!CanDmaTransfer(src_ptr, dst_ptr, total_bytes)) {
      size_t left = 0;
      size_t middle = 0;
      size_t right = 0;
      const bool same_ext_mod =
          PtrMod(src_ptr, kExtDmaAlign) == PtrMod(dst_ptr, kExtDmaAlign);
      const bool can_split = SplitForAlignedMiddle(
          src_ptr, dst_ptr, total_bytes, kExtDmaAlign, left, middle, right);
      const roo::byte* src_mid = src_ptr + left;
      roo::byte* dst_mid = dst_ptr + left;
      if (can_split && CanDmaTransfer(src_mid, dst_mid, middle)) {
        if (left > 0) {
          std::memcpy(dst_ptr, src_ptr, left);
        }
        if (right > 0) {
          std::memcpy(dst_ptr + left + middle, src_ptr + left + middle, right);
        }

        ++st.stats.dma_contiguous_split;
        EnqueueDmaOp(st, DmaBlitOp{src_mid, dst_mid, 0, 0, middle, 0});
        return;
      }

      // For small total sizes there may be no aligned middle chunk to DMA even
      // though src/dst share the same ext-DMA modulo. This is expected.
      if (same_ext_mod && !can_split) {
        ++st.stats.fallback_small_contig_no_middle;
        CopySync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
        return;
      }

      ++st.stats.fallback_alignment;
      LogDmaReject("contig", src_ptr, dst_ptr, total_bytes, src_stride,
                   dst_stride, height);
      CopySync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      return;
    }
  } else {
    if (width < kMinRowDmaBytes) {
      ++st.stats.fallback_small_row_below_min_dma;
      CopySync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      return;
    }

    const bool rowwise_direct_dma_ok =
        CanDmaRowwiseTransfer(src_ptr, dst_ptr, width, src_stride, dst_stride);
    if (!rowwise_direct_dma_ok) {
      size_t left = 0;
      size_t middle = 0;
      size_t right = 0;
      const bool same_ext_mod =
          PtrMod(src_ptr, kExtDmaAlign) == PtrMod(dst_ptr, kExtDmaAlign);
      const bool can_split = SplitForAlignedMiddle(
          src_ptr, dst_ptr, width, kExtDmaAlign, left, middle, right);
      const roo::byte* src_mid = src_ptr + left;
      roo::byte* dst_mid = dst_ptr + left;
      if (can_split && CanDmaRowwiseTransfer(src_mid, dst_mid, middle,
                                             src_stride, dst_stride)) {
        if (left > 0) {
          CopySync(src_ptr, src_stride, dst_ptr, dst_stride, left, height);
        }
        if (right > 0) {
          CopySync(src_ptr + left + middle, src_stride, dst_ptr + left + middle,
                   dst_stride, right, height);
        }

        ++st.stats.dma_rowwise_split;
        EnqueueDmaOp(st, DmaBlitOp{src_mid, dst_mid, src_stride, dst_stride,
                                   middle, height});
        return;
      }

      // For small widths there may be no aligned middle chunk to DMA even
      // though src/dst share the same ext-DMA modulo. This is expected.
      if (same_ext_mod && !can_split) {
        ++st.stats.fallback_small_row_no_middle;
        CopySync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
        return;
      }

      ++st.stats.fallback_alignment;
      LogDmaReject("row", src_ptr, dst_ptr, width, src_stride, dst_stride,
                   height);
      CopySync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      return;
    }
  }
  if (contiguous) {
    ++st.stats.dma_contiguous;
    EnqueueDmaOp(st, DmaBlitOp{src_ptr, dst_ptr, 0, 0, width * height, 0});
  } else {
    ++st.stats.dma_rowwise;
    EnqueueDmaOp(
        st, DmaBlitOp{src_ptr, dst_ptr, src_stride, dst_stride, width, height});
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

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

namespace roo_display {
namespace {

constexpr size_t kDmaAlign = 16;
constexpr size_t kExtDmaAlign = 32;
constexpr uint32_t kStatsLogInterval = 120;
constexpr const char* kTag = "async_blit";

struct AsyncBlitStats {
  uint32_t total_requests = 0;
  uint32_t dma_contiguous = 0;
  uint32_t dma_rowwise = 0;
  uint32_t fallback_no_handle_or_busy = 0;
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
  std::function<void()> done;
  bool busy = false;
  AsyncBlitStats stats;
};

AsyncBlitState& state() {
  static AsyncBlitState s;
  return s;
}

void maybe_log_stats(AsyncBlitState& st) {
  if (st.stats.total_requests % kStatsLogInterval != 0) return;
  ESP_LOGI(kTag,
           "req=%u dma_contig=%u dma_rows=%u fb_busy=%u fb_align=%u fb_err=%u",
           st.stats.total_requests, st.stats.dma_contiguous,
           st.stats.dma_rowwise, st.stats.fallback_no_handle_or_busy,
           st.stats.fallback_alignment, st.stats.fallback_esp_err);
}

void copy_remaining_rows_sync(AsyncBlitState& st) {
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

bool on_copy_done(async_memcpy_handle_t, async_memcpy_event_t*, void* cb_args) {
  auto* st = static_cast<AsyncBlitState*>(cb_args);
  if (st == nullptr) return false;

  if (st->remaining_rows == 0) {
    auto done = std::move(st->done);
    st->busy = false;
    if (done) done();
    return false;
  }

  st->src += st->src_stride;
  st->dst += st->dst_stride;
  --st->remaining_rows;

  if (st->remaining_rows == 0) {
    auto done = std::move(st->done);
    st->busy = false;
    if (done) done();
    return false;
  }

  esp_err_t err =
      esp_async_memcpy(st->handle, st->dst, const_cast<roo::byte*>(st->src),
                       st->row_bytes, on_copy_done, st);
  if (err != ESP_OK) {
    copy_remaining_rows_sync(*st);
    auto done = std::move(st->done);
    st->busy = false;
    if (done) done();
  }
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
  if (st.handle != nullptr && !st.busy) {
    esp_async_memcpy_uninstall(st.handle);
    st.handle = nullptr;
  }
}

void async_blit(const roo::byte* src_ptr, size_t src_stride, roo::byte* dst_ptr,
                size_t dst_stride, size_t width, size_t height,
                std::function<void()> cb) {
  if (src_ptr == nullptr || dst_ptr == nullptr || width == 0 || height == 0) {
    if (cb) cb();
    return;
  }

  AsyncBlitState& st = state();
  ++st.stats.total_requests;
  maybe_log_stats(st);
  const bool contiguous = (src_stride == width && dst_stride == width);
  if (st.handle == nullptr || st.busy) {
    ++st.stats.fallback_no_handle_or_busy;
    copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
    if (cb) cb();
    return;
  }

  if (contiguous) {
    if (!can_dma_transfer(src_ptr, dst_ptr, width * height)) {
      ++st.stats.fallback_alignment;
      log_dma_reject("contig", src_ptr, dst_ptr, width * height, src_stride,
                     dst_stride, height);
      copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      if (cb) cb();
      return;
    }
  } else {
    if (!can_dma_transfer(src_ptr, dst_ptr, width) ||
        !are_strides_aligned(src_stride, dst_stride, src_ptr, dst_ptr)) {
      ++st.stats.fallback_alignment;
      log_dma_reject("row", src_ptr, dst_ptr, width, src_stride, dst_stride,
                     height);
      copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
      if (cb) cb();
      return;
    }
  }

  st.src = src_ptr;
  st.dst = dst_ptr;
  st.src_stride = src_stride;
  st.dst_stride = dst_stride;
  st.row_bytes = width;
  st.remaining_rows = height;
  st.done = std::move(cb);
  st.busy = true;

  esp_err_t err;
  if (contiguous) {
    ++st.stats.dma_contiguous;
    st.remaining_rows = 0;
    err = esp_async_memcpy(st.handle, st.dst, const_cast<roo::byte*>(st.src),
                           width * height, on_copy_done, &st);
  } else {
    ++st.stats.dma_rowwise;
    err = esp_async_memcpy(st.handle, st.dst, const_cast<roo::byte*>(st.src),
                           st.row_bytes, on_copy_done, &st);
  }

  if (err != ESP_OK) {
    ++st.stats.fallback_esp_err;
    st.busy = false;
    copy_sync(src_ptr, src_stride, dst_ptr, dst_stride, width, height);
    auto done = std::move(st.done);
    if (done) done();
  }
}

}  // namespace roo_display

#else

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

#endif

#endif  // defined(ESP_PLATFORM)

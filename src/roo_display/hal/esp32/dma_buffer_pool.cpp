#include "roo_display/hal/esp32/dma_buffer_pool.h"

#include <cstddef>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "roo_logging.h"

namespace roo_display {
namespace esp32 {

DmaBufferPool::DmaBufferPool()
    : buffer_data_(nullptr),
      available_(nullptr),
      mux_(portMUX_INITIALIZER_UNLOCKED) {
  for (size_t i = 0; i < kDmaBufferCount; ++i) {
    in_use_[i] = false;
  }
}

void DmaBufferPool::lockState() {
  if (xPortInIsrContext()) {
    portENTER_CRITICAL_ISR(&mux_);
  } else {
    portENTER_CRITICAL(&mux_);
  }
}

void DmaBufferPool::unlockState() {
  if (xPortInIsrContext()) {
    portEXIT_CRITICAL_ISR(&mux_);
  } else {
    portEXIT_CRITICAL(&mux_);
  }
}

size_t DmaBufferPool::acquireFreeSlot() {
  lockState();
  for (size_t i = 0; i < kDmaBufferCount; ++i) {
    if (!in_use_[i]) {
      in_use_[i] = true;
      unlockState();
      return i;
    }
  }
  unlockState();
  CHECK(false) << "DmaBufferPool semaphore/signaling mismatch";
  return 0;
}

bool DmaBufferPool::markSlotFree(size_t index) {
  bool released = false;
  lockState();
  if (in_use_[index]) {
    in_use_[index] = false;
    released = true;
  }
  unlockState();
  return released;
}

void DmaBufferPool::begin() {
  CHECK_EQ(buffer_data_, nullptr);
  CHECK_EQ(available_, nullptr);

  const size_t pool_bytes = kDmaBufferCount * kDmaBufferCapacity;
  buffer_data_ = static_cast<roo::byte*>(
      heap_caps_malloc(pool_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  CHECK_NOTNULL(buffer_data_);

  available_ = xSemaphoreCreateCountingStatic(kDmaBufferCount, kDmaBufferCount,
                                              &available_storage_);
  CHECK_NOTNULL(available_);

  for (size_t i = 0; i < kDmaBufferCount; ++i) {
    in_use_[i] = false;
  }
}

void DmaBufferPool::end() {
  if (buffer_data_ == nullptr) return;

  CHECK_NOTNULL(available_);
  vSemaphoreDelete(available_);
  available_ = nullptr;

  heap_caps_free(buffer_data_);
  buffer_data_ = nullptr;
}

DmaBufferPool::Buffer DmaBufferPool::acquire() {
  CHECK_NOTNULL(buffer_data_);
  CHECK_NOTNULL(available_);

  CHECK_EQ(xSemaphoreTake(available_, portMAX_DELAY), pdTRUE);
  size_t index = acquireFreeSlot();
  CHECK_LT(index, kDmaBufferCount);

  return Buffer{.data = buffer_data_ + index * kDmaBufferCapacity};
}

void DmaBufferPool::release(Buffer buffer) {
  CHECK_NOTNULL(buffer_data_);
  CHECK_NOTNULL(available_);
  CHECK_NOTNULL(buffer.data);

  ptrdiff_t offset = buffer.data - buffer_data_;
  CHECK_GE(offset, 0);
  CHECK_EQ(offset % static_cast<ptrdiff_t>(kDmaBufferCapacity), 0);

  size_t index = static_cast<size_t>(offset) / kDmaBufferCapacity;
  CHECK_LT(index, kDmaBufferCount);
  CHECK(markSlotFree(index))
      << "Buffer released twice or not acquired from this pool";

  if (xPortInIsrContext()) {
    BaseType_t high_wakeup = pdFALSE;
    CHECK_EQ(xSemaphoreGiveFromISR(available_, &high_wakeup), pdTRUE);
    if (high_wakeup == pdTRUE) {
      portYIELD_FROM_ISR();
    }
    return;
  }

  CHECK_EQ(xSemaphoreGive(available_), pdTRUE);
}

}  // namespace esp32
}  // namespace roo_display
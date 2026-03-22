#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "roo_backport.h"
#include "roo_backport/byte.h"

#ifndef ROO_DISPLAY_ESP32_DMA_BUFFER_CAPACITY
#define ROO_DISPLAY_ESP32_DMA_BUFFER_CAPACITY 1024
#endif

#ifndef ROO_DISPLAY_ESP32_DMA_BUFFER_COUNT
#define ROO_DISPLAY_ESP32_DMA_BUFFER_COUNT 4
#endif

namespace roo_display {
namespace esp32 {

static constexpr size_t kDmaBufferCapacity =
    ROO_DISPLAY_ESP32_DMA_BUFFER_CAPACITY;

static constexpr size_t kDmaBufferCount = ROO_DISPLAY_ESP32_DMA_BUFFER_COUNT;

// Assumes a single producer (the SPI transaction code), acquiring the buffers,
// and a single consumer (the SPI ISR), releasing the buffers. The producer can
// get blocked if all buffers are in use.
class DmaBufferPool {
 public:
  struct Buffer {
    roo::byte* data;
    size_t size;
  };

  DmaBufferPool();

  // Allocates buffer data, in a contiguous block of DMA-capable DRAM.
  void begin();

  // Deallocates the buffer data.
  void end();

  // Returns a pointer to a buffer to write to. If no buffer is currently
  // available, the caller task gets blocked until one becomes available.
  Buffer acquire();

  // Marks the buffer as available for reuse. The buffer must have been
  // previously returned by acquire() and not yet released.
  void release(Buffer buffer);

  size_t pool_capacity() const { return kDmaBufferCount; }
  size_t buffer_size() const { return kDmaBufferCapacity; }

 private:
  void lockState();
  void unlockState();
  size_t acquireFreeSlot();
  bool markSlotFree(size_t index);

  roo::byte* buffer_data_;
  StaticSemaphore_t available_storage_;
  SemaphoreHandle_t available_;
  bool in_use_[kDmaBufferCount];
  portMUX_TYPE mux_;
};

}  // namespace esp32
}  // namespace roo_display

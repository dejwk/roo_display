#pragma once

#include <memory>

#include "roo_display/hal/esp32/dma_buffer_pool.h"

namespace roo_display {
namespace esp32 {

template <typename T>
class RingBuf {
 public:
  RingBuf(size_t capacity)
      : buf_(new T[capacity]), capacity_(capacity), head_(0), used_(0) {}

  size_t capacity() const { return capacity_; }
  size_t used() const { return used_; }
  size_t free() const { return capacity_ - used_; }

  bool full() const { return used_ == capacity_; }
  bool empty() const { return used_ == 0; }

  void clear() {
    head_ = 0;
    used_ = 0;
  }

  // Writes a single item to the ringbuffer. Returns true if successful, or
  // false if the buffer is full.
  bool write(T item) {
    if (used_ == capacity_) return false;
    buf_[write_pos()] = std::move(item);
    used_++;
    return true;
  }

  // Reads a single item from the ringbuffer. Returns true if successful, or
  // false if the buffer is empty.
  bool read(T& item) {
    if (used_ == 0) return false;
    item = std::move(buf_[head_]);
    head_++;
    if (head_ == capacity_) head_ = 0;
    used_--;
    return true;
  }

 private:
  size_t write_pos() const {
    size_t pos = head_ + used_;
    if (pos > capacity_) pos -= capacity_;
    return pos;
  }

  std::unique_ptr<T[]> buf_;
  size_t capacity_;
  size_t head_;
  size_t used_;
};

class DmaController {
 public:
  struct Operation {
    const roo::byte* out_data;
    size_t out_len;
  };

  DmaController(DmaBufferPool& dma_buffer_pool);

  // Initializes the DMA controller. This method must be called before any other
  // method. The caller must ensure that the dma_buffer_pool remains valid for
  // the entire duration of use of the DmaController.
  void begin();

  // Deinitializes the DMA controller. After this method returns, the caller
  // must not call any other method. The caller must ensure that there are no
  // pending DMA operations before calling this method.
  void end();

  // Submits a DMA operation. The caller must ensure that the data buffer
  // remains valid until the operation completes. The caller must ensure that
  // the out_len is aligned to 32 bits.
  //
  // If there is no pending DMA operation, the submitted operation gets started
  // immediately. Otherwise, the submitted operation gets enqueued and will
  // start when all previously submitted operations complete. The caller can
  // submit multiple operations in a row, and they will get executed in order.
  // It is the caller's responsibility to ensure that the total number of
  // pending operations does not exceed the capacity of the dma_buffer_pool_.
  //
  // If the DMA operation fails to start, this method returns false.
  bool submit(Operation op);

  // Blocks until all previously submitted operations complete. If there are no
  // pending operations, returns immediately. Only one task can wait for
  // completion at a time; if multiple tasks call this method concurrently, the
  // behavior is undefined.
  void awaitCompleted();

 private:
  // The DMA ISR callback.
  friend void IRAM_ATTR DmaTransferCompleteISR(const void*);

  // Called from the SPI ISR when a DMA operation completes. This method marks
  // the current operation as complete, returns the buffer to the pool, and
  // starts the next pending operation, if any. If there are no more operations,
  // the potential waiter gets notified. This method must not be called directly
  // from application code.
  void transferCompleteISR();

  DmaBufferPool& dma_buffer_pool_;
  RingBuf<Operation> pending_ops_;
};

}  // namespace esp32
}  // namespace roo_display
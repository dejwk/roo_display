#pragma once

#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/spi_types.h"
#include "roo_display/hal/esp32/dma_buffer_pool.h"
#include "roo_display/hal/esp32/spi_irq.h"

#if __has_include("esp_private/spi_common_internal.h")
#include "esp_private/spi_common_internal.h"
#define ROO_DISPLAY_HAS_SPI_INTERNAL_DMA 1
#else
#define ROO_DISPLAY_HAS_SPI_INTERNAL_DMA 0
#endif

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
    if (pos >= capacity_) pos -= capacity_;
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
    // Pointer to a buffer acquired from this controller's DmaBufferPool.
    // Ownership transfers to DmaController on submit().
    const roo::byte* out_data;

    // Maximum number of bytes transmitted per DMA start for this operation.
    // Must be > 0 and 4-byte aligned.
    size_t out_size;

    // Total number of bytes to transmit by repeating out_data as needed.
    // Must be > 0 and 4-byte aligned.
    size_t remaining;
  };

  // Acquires a DMA-capable buffer from the internal pool.
  //
  // Internal contract: buffers acquired here are intended to be submitted via
  // submit(). This call may block if all buffers are in use by in-flight
  // operations.
  DmaBufferPool::Buffer acquireBuffer();

  // Releases a previously acquired buffer back to the pool.
  //
  // Must only be used for buffers that have not been submitted. Submitted
  // buffers are released by DmaController on completion.
  void releaseBuffer(DmaBufferPool::Buffer buffer);

  size_t bufferCapacity() const { return dma_buffer_pool_.buffer_size(); }

  // Binds this controller's transfer-complete ISR callback to the SPI host IRQ
  // dispatcher. Must succeed before submit() is used.
  bool bindInterrupt();

  // Unbinds this controller's ISR callback from the SPI host IRQ dispatcher.
  // Call only when no DMA operation is in flight.
  void unbindInterrupt();

  // Submits a DMA operation.
  //
  // Internal contract:
  // - bindInterrupt() must have succeeded before calling submit().
  // - op.out_data must point to a buffer acquired from acquireBuffer().
  // - Ownership of op.out_data transfers to DmaController on success; caller
  //   must not release or reuse that buffer.
  //
  // If there is no pending DMA operation, the submitted operation gets started
  // immediately. Otherwise, the submitted operation gets enqueued and will
  // start when all previously submitted operations complete. The caller can
  // submit multiple operations in a row, and they will get executed in order.
  // In normal usage, the buffer pool bounds in-flight submissions by blocking
  // acquireBuffer() when all pool buffers are busy.
  //
  // If the DMA operation fails to start, this method returns false.
  bool submit(Operation op);

  // Blocks until all previously submitted operations complete and submitted
  // buffers are returned to the pool. If there are no pending operations,
  // returns immediately.
  //
  // Only one task can wait for completion at a time; if multiple tasks call
  // this method concurrently, the behavior is undefined.
  void awaitCompleted();

 private:
  friend DmaController* GetDmaControllerForHost(int spi_port);

  // The DMA ISR callback.
  friend void IRAM_ATTR DmaTransferCompleteISR(void*);

  DmaController(spi_host_device_t host_id, DmaBufferPool& dma_buffer_pool);

  // Initializes the DMA controller. This method must be called before any other
  // method. The caller must ensure that the dma_buffer_pool remains valid for
  // the entire duration of use of the DmaController.
  void begin();

  // Deinitializes the DMA controller. After this method returns, the caller
  // must not call any other method. The caller must ensure that there are no
  // pending DMA operations before calling this method.
  void end();

  // Called from the SPI ISR when a DMA operation completes. This method marks
  // the current operation as complete, returns the buffer to the pool, and
  // starts the next pending operation, if any. If there are no more operations,
  // the potential waiter gets notified. This method must not be called directly
  // from application code.
  void transferCompleteISR();

#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  // Resets runtime queue/processing state to idle.
  void resetRuntimeState();

  // Starts a DMA operation. Must be called while holding mux_ (task or ISR
  // critical section variant).
  bool prepareOperationCritical(Operation op);

  bool startOperation();
#endif

  // SPI host this controller is bound to.
  spi_host_device_t host_id_;

#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  // Active DMA context used for transfers; may be borrowed from SPI bus or
  // point to owned_dma_ctx_.
  const spi_dma_ctx_t* dma_ctx_;

  // DMA context allocated by this controller (if bus-level context is absent,
  // e.g. in case of Arduino SPI). Freed in end(); null when dma_ctx_ is
  // borrowed.
  spi_dma_ctx_t* owned_dma_ctx_;

  IrqDispatcher* irq_dispatcher_;

  IrqDispatcher::Binding irq_binding_;

  // // Handle returned by esp_intr_alloc for the SPI host interrupt.
  // // Guarded by mux_.
  // intr_handle_t dma_intr_handle_;

  // True while a DMA queue is getting processed. Set to true atomically
  // when the first DMA operation gets scheduled; cleared when the queue gets
  // empty.
  bool processing_;

  // Task currently blocked in awaitCompleted(), if any.
  // Guarded by mux_.
  TaskHandle_t waiter_task_;

  // Operation currently being transferred by DMA.
  // Guarded by mux_.
  Operation current_op_;

  // Number of bytes in the current transfer.
  // Guarded by mux_.
  size_t current_op_size_;

  // Number of remaining bytes after current_op_ completes.
  // Guarded by mux_.
  size_t current_op_remaining_;

  // Protects ISR/task shared state above.
  portMUX_TYPE mux_;
#endif

  // Buffer pool backing operation payload storage.
  DmaBufferPool& dma_buffer_pool_;

  // FIFO of submitted operations waiting to start.
  // Guarded by mux_.
  RingBuf<Operation> pending_ops_;
};

DmaController* GetDmaControllerForHost(int spi_port);

}  // namespace esp32
}  // namespace roo_display
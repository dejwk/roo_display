#if (defined ESP_PLATFORM) && !defined(ROO_TESTING)

#include "roo_display/hal/esp32/spi_async.h"

namespace roo_display {
namespace esp32 {

namespace {

template <int spi_port>
void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline)) SpiTxStartISR() {
  SpiTxStart(spi_port);
}

template <int spi_port>
void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline))
SpiWrite64AlignedISR(const roo::byte* src) {
  SpiWrite64Aligned(spi_port, src);
}

template <int spi_port>
void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline))
SpiWriteUpTo64AlignedISR(const roo::byte* src, int len) {
  SpiWriteUpTo64Aligned(spi_port, src, len);
}

template <int spi_port>
void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline))
SpiSetOutBufferSizeISR(int dlen) {
  SpiSetOutBufferSize(spi_port, dlen);
}

template <int spi_port>
void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline))
SpiTransferDoneIntDisableISR() {
  SpiTransferDoneIntDisable(spi_port);
}

}  // namespace

template <int spi_port>
void AsyncOperation<spi_port>::TransferCompleteISR(void* arg) {
  AsyncOperation<spi_port>* op = static_cast<AsyncOperation<spi_port>*>(arg);
  op->handleInterrupt();
}

AsyncOperationBase::AsyncOperationBase()
    : type_(kFill),
      done_(true),
      stop_(false),
      mux_(portMUX_INITIALIZER_UNLOCKED),
      waiter_task_(nullptr) {}

template <int spi_port>
AsyncOperation<spi_port>::AsyncOperation() : AsyncOperationBase() {}

void AsyncOperationBase::initFill(size_t len) {
  done_ = false;
  type_ = kFill;
  stop_ = false;
  fill_.init(len);
}

void AsyncOperationBase::initBlit(const roo::byte* data,
                                  size_t row_stride_bytes, size_t row_bytes,
                                  size_t row_count) {
  done_ = false;
  type_ = kBlit;
  stop_ = false;
  blit_.init(data, row_stride_bytes, row_bytes, row_count);
}

template <int spi_port>
void AsyncOperation<spi_port>::initBlit(const roo::byte* data,
                                        size_t row_stride_bytes,
                                        size_t row_bytes, size_t row_count) {
  AsyncOperationBase::initBlit(data, row_stride_bytes, row_bytes, row_count);
  prepareNextBlitChunk();
}

template <int spi_port>
void AsyncOperation<spi_port>::handleInterrupt() {
  if (checkCompleteISR()) {
    SpiTransferDoneIntDisableISR<spi_port>();
    markDoneAndNotifyWaiterISR();
    return;
  }
  SpiTxStartISR<spi_port>();
}

void AsyncOperationBase::awaitCompletion(bool eager_completion) {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  while (true) {
    portENTER_CRITICAL(&mux_);
    if (eager_completion) {
      stop_ = true;
    }
    if (done_ || eager_completion) {
      waiter_task_ = nullptr;
      portEXIT_CRITICAL(&mux_);
      return;
    }
    CHECK(waiter_task_ == nullptr || waiter_task_ == me);
    waiter_task_ = me;
    portEXIT_CRITICAL(&mux_);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

template <int spi_port>
void AsyncOperation<spi_port>::awaitCompletion(bool eager_completion) {
  if (!eager_completion) {
    AsyncOperationBase::awaitCompletion(false);
    return;
  }

  // Mode 1: intercept async progression and finish remaining chunks
  // synchronously in task context. Let any currently in-flight chunk complete
  // at the hardware level, then disable transfer-done IRQ progression.
  bool already_done;
  portENTER_CRITICAL(&mux_);
  stop_ = true;
  already_done = done_;
  waiter_task_ = nullptr;
  portEXIT_CRITICAL(&mux_);

  if (!already_done) {
    SpiTxWait(spi_port);
    SpiTransferDoneIntDisable(spi_port);
    SpiTransferDoneIntClear(spi_port);
    portENTER_CRITICAL(&mux_);
    done_ = true;
    waiter_task_ = nullptr;
    portEXIT_CRITICAL(&mux_);
  }

  finishRemaining();
}

const roo::byte* AsyncOperationBase::Blit::nextChunk(roo::byte* scratch,
                                                     size_t& len) {
  if (remaining_rows == 0 || next_row == nullptr) {
    chunk_size = 0;
    len = 0;
    return nullptr;
  }
  size_t in_row = row_bytes - row_offset;
  const roo::byte* src = next_row + row_offset;
  if (in_row >= 64 && (reinterpret_cast<uintptr_t>(src) & 0x3u) == 0) {
    // Aligned.
    len = 64;
    chunk_size = 64;
    row_offset += 64;
    if (row_offset == row_bytes) {
      row_offset = 0;
      next_row += row_stride_bytes;
      --remaining_rows;
    }
    return src;
  }

  // Chunked.
  len = 0;
  while (len < 64 && remaining_rows > 0) {
    size_t row_remaining = row_bytes - row_offset;
    size_t take = (64 - len < row_remaining) ? (64 - len) : row_remaining;
    memcpy_isr(&scratch[len], next_row + row_offset, take);
    len += take;
    row_offset += take;
    if (row_offset == row_bytes) {
      row_offset = 0;
      next_row += row_stride_bytes;
      --remaining_rows;
    }
  }
  chunk_size = len;
  if (len == 0) return nullptr;
  return scratch;
}

template <int spi_port>
bool AsyncOperation<spi_port>::prepareNextBlitChunk() {
  alignas(4) roo::byte scratch[64];
  size_t len;
  const roo::byte* chunk = blit_.nextChunk(scratch, len);
  if (len == 0) return false;
  if (len < 64) {
    SpiSetOutBufferSizeISR<spi_port>(len);
    SpiWriteUpTo64AlignedISR<spi_port>(chunk, len);
  } else {
    SpiWrite64AlignedISR<spi_port>(chunk);
  }
  return true;
}

template <int spi_port>
void AsyncOperation<spi_port>::markDoneAndNotifyWaiterISR() {
  done_ = true;
  TaskHandle_t waiter = waiter_task_;
  waiter_task_ = nullptr;
  portEXIT_CRITICAL_ISR(&mux_);
  if (waiter != nullptr) {
    BaseType_t high_wakeup = pdFALSE;
    vTaskNotifyGiveFromISR(waiter, &high_wakeup);
    portYIELD_FROM_ISR(high_wakeup);
  }
}

template <int spi_port>
bool AsyncOperation<spi_port>::checkCompleteISR() {
  portENTER_CRITICAL_ISR(&mux_);
  if (stop_) {
    return true;
  }
  if (type_ == kFill) {
    if (fill_.nextChunk()) {
      portEXIT_CRITICAL_ISR(&mux_);
      return false;
    }
    return true;
  }
  if (type_ == kBlit) {
    if (prepareNextBlitChunk()) {
      portEXIT_CRITICAL_ISR(&mux_);
      return false;
    }
    return true;
  }
  portEXIT_CRITICAL_ISR(&mux_);
  return false;
}

template <int spi_port>
void AsyncOperation<spi_port>::finishFill() {
  while (fill_.nextChunk()) {
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }
}

template <int spi_port>
void AsyncOperation<spi_port>::finishBlit() {
  while (prepareNextBlitChunk()) {
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
  }
}

template <int spi_port>
void AsyncOperation<spi_port>::finishRemaining() {
  if (type_ == kFill) {
    finishFill();
  } else if (type_ == kBlit) {
    finishBlit();
  }
}

template class AsyncOperation<2>;
template class AsyncOperation<3>;

}  // namespace esp32
}  // namespace roo_display

#endif  // defined(ESP_PLATFORM) && !defined(ROO_TESTING)

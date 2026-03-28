#include "roo_display/hal/esp32/spi_async.h"

#ifndef ROO_TESTING

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
SpiNonDmaTransferDoneIntDisableISR() {
  SpiNonDmaTransferDoneIntDisable(spi_port);
}

}  // namespace

template <int spi_port>
void AsyncOperation<spi_port>::TransferCompleteISR(void* arg) {
  AsyncOperation<spi_port>* op = static_cast<AsyncOperation<spi_port>*>(arg);
  op->handleInterrupt();
}

template <int spi_port>
AsyncOperation<spi_port>::AsyncOperation()
    : type_(kFill),
      done_(true),
      stop_(false),
      mux_(portMUX_INITIALIZER_UNLOCKED),
      waiter_task_(nullptr) {}

template <int spi_port>
void AsyncOperation<spi_port>::initFill(size_t len) {
  done_ = false;
  type_ = kFill;
  fill_.remaining = len;
  stop_ = false;
}

template <int spi_port>
void AsyncOperation<spi_port>::initBlit(const roo::byte* data,
                                        size_t row_stride_bytes,
                                        size_t row_bytes, size_t row_count) {
  done_ = false;
  type_ = kBlit;
  blit_.next_row = data;
  blit_.row_stride_bytes = row_stride_bytes;
  blit_.row_bytes = row_bytes;
  blit_.remaining_rows = row_count;
  blit_.row_offset = 0;
  blit_.chunk_size = 0;
  prepareNextBlitChunk();
  stop_ = false;
}

template <int spi_port>
void AsyncOperation<spi_port>::handleInterrupt() {
  if (checkCompleteISR()) return;
  SpiTxStartISR<spi_port>();
}

template <int spi_port>
void AsyncOperation<spi_port>::awaitCompletion() {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  while (true) {
    portENTER_CRITICAL(&mux_);
    stop_ = true;
    if (done_) {
      waiter_task_ = nullptr;
      portEXIT_CRITICAL(&mux_);
      finishRemaining();
      return;
    }
    CHECK(waiter_task_ == nullptr || waiter_task_ == me);
    waiter_task_ = me;
    portEXIT_CRITICAL(&mux_);
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}

template <int spi_port>
bool AsyncOperation<spi_port>::loadAlignedBlitSlice(const roo::byte* src) {
  SpiWrite64AlignedISR<spi_port>(src);
  blit_.chunk_size = 64;
  blit_.row_offset += 64;
  if (blit_.row_offset == blit_.row_bytes) {
    blit_.row_offset = 0;
    blit_.next_row += blit_.row_stride_bytes;
    --blit_.remaining_rows;
  }
  return true;
}

template <int spi_port>
size_t AsyncOperation<spi_port>::buildChunkedBlitSlice(roo::byte* chunk) {
  size_t out = 0;
  while (out < 64 && blit_.remaining_rows > 0) {
    size_t row_remaining = blit_.row_bytes - blit_.row_offset;
    size_t take = (64 - out < row_remaining) ? (64 - out) : row_remaining;
    memcpy_isr(&chunk[out], blit_.next_row + blit_.row_offset, take);
    out += take;
    blit_.row_offset += take;
    if (blit_.row_offset == blit_.row_bytes) {
      blit_.row_offset = 0;
      blit_.next_row += blit_.row_stride_bytes;
      --blit_.remaining_rows;
    }
  }
  return out;
}

template <int spi_port>
void AsyncOperation<spi_port>::writeChunkedBlitSlice(const roo::byte* chunk,
                                                     size_t len) {
  if (len < 64) {
    SpiSetOutBufferSizeISR<spi_port>(len);
  }
  SpiWriteUpTo64AlignedISR<spi_port>(chunk, len);
  blit_.chunk_size = len;
}

template <int spi_port>
bool AsyncOperation<spi_port>::loadChunkedBlitSlice() {
  alignas(4) roo::byte chunk[64];
  size_t out = buildChunkedBlitSlice(chunk);
  if (out == 0) return false;
  writeChunkedBlitSlice(chunk, out);
  return true;
}

template <int spi_port>
bool AsyncOperation<spi_port>::prepareNextBlitChunk() {
  if (blit_.remaining_rows == 0 || blit_.next_row == nullptr) {
    blit_.chunk_size = 0;
    return false;
  }

  size_t in_row = blit_.row_bytes - blit_.row_offset;
  const roo::byte* src = blit_.next_row + blit_.row_offset;
  if (in_row >= 64 && (reinterpret_cast<uintptr_t>(src) & 0x3u) == 0) {
    return loadAlignedBlitSlice(src);
  }
  return loadChunkedBlitSlice();
}

template <int spi_port>
void AsyncOperation<spi_port>::markDoneAndNotifyWaiterISR() {
  SpiNonDmaTransferDoneIntDisableISR<spi_port>();
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
  if (type_ == kFill) {
    portENTER_CRITICAL_ISR(&mux_);
    if (!stop_ && fill_.remaining > 64) {
      fill_.remaining -= 64;
      portEXIT_CRITICAL_ISR(&mux_);
      return false;
    }
    if (!stop_) fill_.remaining = 0;
    markDoneAndNotifyWaiterISR();
    return true;
  }

  if (type_ == kBlit) {
    portENTER_CRITICAL_ISR(&mux_);
    if (stop_) {
      markDoneAndNotifyWaiterISR();
      return true;
    }
    if (prepareNextBlitChunk()) {
      portEXIT_CRITICAL_ISR(&mux_);
      return false;
    }
    markDoneAndNotifyWaiterISR();
    return true;
  }
  return true;
}

template <int spi_port>
void AsyncOperation<spi_port>::finishFill() {
  size_t count = fill_.remaining;
  while (count > 0) {
    SpiTxStart(spi_port);
    count -= 64;
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

#endif  // ROO_TESTING

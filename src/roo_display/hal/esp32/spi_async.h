#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#endif

#include <cstddef>
#include <cstdint>

#include "roo_backport/byte.h"
#include "roo_display/hal/esp32/memory.h"
#include "roo_display/hal/esp32/spi_irq.h"
#include "roo_display/hal/esp32/spi_reg.h"
#include "roo_logging.h"
#include "soc/spi_reg.h"

#ifndef ROO_TESTING

namespace roo_display {
namespace esp32 {

class AsyncOperationBase {
 protected:
  AsyncOperationBase();

 public:
  void initFill(size_t len);

  void initBlit(const roo::byte* data, size_t row_stride_bytes,
                size_t row_bytes, size_t row_count);

  void awaitCompletion(bool eager_completion);

 protected:
  enum Type { kFill, kBlit };

  struct Fill {
    size_t remaining;

    void init(size_t len) { remaining = len; }

    bool ROO_DISPLAY_SPI_ASYNC_ISR_ATTR nextChunk() {
      if (remaining <= 64) {
        remaining = 0;
        return false;
      }
      remaining -= 64;
      return true;
    }
  };

  struct Blit {
    const roo::byte* next_row;
    size_t row_stride_bytes;
    size_t row_bytes;
    size_t remaining_rows;
    size_t row_offset;
    size_t chunk_size;

    void init(const roo::byte* data, size_t row_stride_bytes, size_t row_bytes,
              size_t row_count) {
      this->next_row = data;
      this->row_stride_bytes = row_stride_bytes;
      this->row_bytes = row_bytes;
      this->remaining_rows = row_count;
      this->row_offset = 0;
      this->chunk_size = 0;
    }

    const roo::byte* ROO_DISPLAY_SPI_ASYNC_ISR_ATTR nextChunk(roo::byte* scrach,
                                                              size_t& len);
  };

  Type type_;
  union {
    Fill fill_;
    Blit blit_;
  };
  bool done_;
  bool stop_;
  portMUX_TYPE mux_;
  TaskHandle_t waiter_task_;
};

template <int spi_port>
class AsyncOperation : public AsyncOperationBase {
 public:
  AsyncOperation();

  static void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline))
  TransferCompleteISR(void* arg);

  void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR __attribute__((noinline))
  handleInterrupt();

  void initBlit(const roo::byte* data, size_t row_stride_bytes,
                size_t row_bytes, size_t row_count);

  void awaitCompletion(bool eager_completion);

 private:
  void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR loadAlignedBlitSlice(const roo::byte* src)
      __attribute__((noinline));

  size_t ROO_DISPLAY_SPI_ASYNC_ISR_ATTR buildChunkedBlitSlice(roo::byte* chunk)
      __attribute__((noinline));

  void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR WriteChunkedBlitWords(const uint32_t* d32,
                                                            int words, int dlen)
      __attribute__((noinline));

  void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR writeChunkedBlitSlice(
      const roo::byte* chunk, size_t out) __attribute__((noinline));

  bool ROO_DISPLAY_SPI_ASYNC_ISR_ATTR loadChunkedBlitSlice()
      __attribute__((noinline));

  bool ROO_DISPLAY_SPI_ASYNC_ISR_ATTR prepareNextBlitChunk()
      __attribute__((noinline));

  void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR markDoneAndNotifyWaiterISR()
      __attribute__((noinline));

  bool ROO_DISPLAY_SPI_ASYNC_ISR_ATTR checkCompleteISR()
      __attribute__((noinline));

  void finishFill();

  void finishBlit();

  void finishRemaining();
};

extern template class AsyncOperation<2>;
extern template class AsyncOperation<3>;

}  // namespace esp32
}  // namespace roo_display

#endif  // ROO_TESTING

#pragma once

#include <cstddef>
#include <cstdint>

#include "roo_backport/byte.h"
#include "roo_display/hal/esp32/spi_async.h"
#include "roo_display/hal/esp32/spi_irq.h"
#include "roo_display/hal/esp32/spi_reg.h"
#include "roo_logging.h"

#ifndef ROO_TESTING

namespace roo_display {
namespace esp32 {

template <uint8_t spi_port>
class NonDmaPipeline {
 public:
  NonDmaPipeline()
      : async_op_(),
        irq_binding_{AsyncOperation<spi_port>::TransferCompleteISR,
                     &async_op_} {}

  bool beginWriteOnlyTransaction() {
    if (irq_bound_) return true;
    if (!bindInterrupt()) return false;
    irq_bound_ = true;
    return true;
  }

  void beginAsyncFill(uint32_t len) {
    DCHECK(irq_bound_);
    async_op_.initFill(len);
    SpiTransferDoneIntClear(spi_port);
    SpiTransferDoneIntEnable(spi_port);
    SpiTxStart(spi_port);
  }

  void beginAsyncBlit(const roo::byte* data, size_t row_stride_bytes,
                      size_t row_bytes, size_t row_count) {
    DCHECK(irq_bound_);
    async_op_.initBlit(data, row_stride_bytes, row_bytes, row_count);
    SpiTransferDoneIntClear(spi_port);
    SpiTransferDoneIntEnable(spi_port);
    SpiSetOutBufferSize(spi_port, 64);
    SpiTxStart(spi_port);
  }

  void flush(bool eager_completion) {
    // Mode 2 keeps completion fully ISR-driven. Other modes eagerly finish in
    // task context when a waiter blocks.
    async_op_.awaitCompletion(eager_completion);
  }

  void endTransaction() {
    unbindInterrupt();
    irq_bound_ = false;
  }

 private:
  bool bindInterrupt() {
    if (irq_dispatcher_ == nullptr) {
      if (irq_alloc_attempted_) return false;
      irq_alloc_attempted_ = true;
      irq_dispatcher_ = GetIrqDispatcher(spi_port);
      if (irq_dispatcher_ == nullptr) return false;
    }
    return irq_dispatcher_->bind(&irq_binding_);
  }

  void unbindInterrupt() {
    if (irq_dispatcher_ != nullptr) {
      irq_dispatcher_->unbind(&irq_binding_);
      irq_dispatcher_ = nullptr;
      irq_alloc_attempted_ = false;
    }
  }

  AsyncOperation<spi_port> async_op_;
  IrqDispatcher::Binding irq_binding_;
  IrqDispatcher* irq_dispatcher_ = nullptr;
  bool irq_alloc_attempted_ = false;
  bool irq_bound_ = false;
};

}  // namespace esp32
}  // namespace roo_display

#endif  // ROO_TESTING

#if defined(ESP_PLATFORM) && !defined(ROO_TESTING)

#include "roo_display/hal/esp32/spi_dma.h"

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "roo_display/hal/esp32/spi_reg.h"
#include "roo_logging.h"

#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA && SOC_GDMA_SUPPORTED
#include "esp_private/gdma.h"
#endif

namespace roo_display {
namespace esp32 {

namespace {

inline uint8_t hostToSpiPort(spi_host_device_t host_id) {
  return static_cast<uint8_t>(host_id) + 1;
}

}  // namespace

void IRAM_ATTR DmaTransferCompleteISR(void* arg) {
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  auto* controller = static_cast<DmaController*>(arg);
  controller->transferCompleteISR();
#else
  (void)arg;
#endif
}

DmaController::DmaController(spi_host_device_t host_id,
                             DmaBufferPool& dma_buffer_pool)
    : host_id_(host_id),
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
      dma_ctx_(nullptr),
      owned_dma_ctx_(nullptr),
      irq_dispatcher_(nullptr),
      irq_binding_{DmaTransferCompleteISR, this},
      transfer_in_progress_(false),
      waiter_task_(nullptr),
      current_op_({nullptr, 0, 0}),
      current_op_repetitions_left_(0),
      mux_(portMUX_INITIALIZER_UNLOCKED),
#endif
      dma_buffer_pool_(dma_buffer_pool),
      pending_ops_(dma_buffer_pool.pool_capacity()) {
}

void DmaController::begin() {
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  if (dma_ctx_ != nullptr && irq_dispatcher_ != nullptr) return;

  transfer_in_progress_ = false;
  waiter_task_ = nullptr;
  current_op_ = {nullptr, 0, 0};
  current_op_repetitions_left_ = 0;
  pending_ops_.clear();

  // Try to acquire the DMA context from the SPI bus.
  dma_ctx_ = spi_bus_get_dma_ctx(host_id_);
  owned_dma_ctx_ = nullptr;
  if (dma_ctx_ != nullptr) {
    // Continue to interrupt setup below.
  } else {
    spi_dma_ctx_t* tmp_ctx = nullptr;
    esp_err_t err =
        spicommon_dma_chan_alloc(host_id_, SPI_DMA_CH_AUTO, &tmp_ctx);
    if (err != ESP_OK || tmp_ctx == nullptr) {
      dma_ctx_ = nullptr;
      return;
    }
    int actual_max_transfer = 0;
    err = spicommon_dma_desc_alloc(tmp_ctx, kDmaBufferCapacity,
                                   &actual_max_transfer);
    if (err != ESP_OK || actual_max_transfer < kDmaBufferCapacity) {
      spicommon_dma_chan_free(tmp_ctx);
      dma_ctx_ = nullptr;
      return;
    }
    owned_dma_ctx_ = tmp_ctx;
    dma_ctx_ = tmp_ctx;
  }

  irq_dispatcher_ = GetIrqDispatcher(hostToSpiPort(host_id_));
  if (irq_dispatcher_ == nullptr) {
    if (owned_dma_ctx_ != nullptr) {
      spicommon_dma_chan_free(owned_dma_ctx_);
      owned_dma_ctx_ = nullptr;
    }
    dma_ctx_ = nullptr;
    return;
  }

  uint8_t spi_port = hostToSpiPort(host_id_);
  SpiDmaTransferDoneIntClear(spi_port);
  SpiDmaTransferDoneIntDisable(spi_port);
  SpiDmaTxDisable(spi_port);
#else
  pending_ops_.clear();
#endif
}

void DmaController::end() {
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  awaitCompleted();

  uint8_t spi_port = hostToSpiPort(host_id_);
  SpiDmaTransferDoneIntDisable(spi_port);
  SpiDmaTxDisable(spi_port);
  SpiDmaTransferDoneIntClear(spi_port);

  irq_dispatcher_ = nullptr;
  if (owned_dma_ctx_ != nullptr) {
    spicommon_dma_chan_free(owned_dma_ctx_);
    owned_dma_ctx_ = nullptr;
  }

  dma_ctx_ = nullptr;
  transfer_in_progress_ = false;
  waiter_task_ = nullptr;
  current_op_ = {nullptr, 0, 0};
  current_op_repetitions_left_ = 0;
  pending_ops_.clear();
#else
  pending_ops_.clear();
#endif
}

bool DmaController::bindInterrupt() {
  if (irq_dispatcher_ == nullptr) return false;
  return irq_dispatcher_->bind(&irq_binding_);
}

void DmaController::unbindInterrupt() {
  if (irq_dispatcher_ == nullptr) return;
  irq_dispatcher_->unbind(&irq_binding_);
}

DmaBufferPool::Buffer DmaController::acquireBuffer() {
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  // if (dma_ctx_ == nullptr || irq_dispatcher_ == nullptr) {
  //   return DmaBufferPool::Buffer{nullptr};
  // }
  return dma_buffer_pool_.acquire();
#else
  return DmaBufferPool::Buffer{nullptr};
#endif
}

void DmaController::releaseBuffer(DmaBufferPool::Buffer buffer) {
  // if (buffer.data == nullptr) return;
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  dma_buffer_pool_.release(buffer);
#else
  (void)buffer;
#endif
}

bool DmaController::submit(Operation op) {
  CHECK_NOTNULL(op.out_data);
  CHECK_GT(op.out_len, 0u);
  CHECK_GT(op.repetitions, 0u);
  CHECK_EQ((op.out_len & 0x3u), 0u);

#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  if (dma_ctx_ == nullptr || irq_dispatcher_ == nullptr) return false;

  bool ok;

  portENTER_CRITICAL(&mux_);
  if (!transfer_in_progress_) {
    ok = startOperationCritical(op);
  } else {
    ok = pending_ops_.write(op);
  }
  portEXIT_CRITICAL(&mux_);

  return ok;
#else
  (void)op;
  return false;
#endif
}

void DmaController::awaitCompleted() {
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  if (dma_ctx_ == nullptr || irq_dispatcher_ == nullptr) return;

  TaskHandle_t self = xTaskGetCurrentTaskHandle();
  while (true) {
    portENTER_CRITICAL(&mux_);
    bool done = !transfer_in_progress_ && pending_ops_.empty();
    if (done) {
      if (waiter_task_ == self) {
        waiter_task_ = nullptr;
      }
      portEXIT_CRITICAL(&mux_);
      return;
    }
    // waiter_task_ = self;
    portEXIT_CRITICAL(&mux_);
    SpiTxWait(hostToSpiPort(host_id_));
    // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
#endif
}

void DmaController::transferCompleteISR() {
#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
  Operation completed = {nullptr, 0, 0};
  Operation next = {nullptr, 0, 0};
  bool has_next = false;
  bool notify_waiter = false;
  TaskHandle_t waiter = nullptr;

  portENTER_CRITICAL_ISR(&mux_);
  if (!transfer_in_progress_) {
    // Can happen on a stale/late IRQ after software already transitioned the
    // controller to idle (e.g. teardown or IRQ/status timing race).
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }

  if (current_op_repetitions_left_ > 1) {
    current_op_repetitions_left_--;
    Operation repeat_op = current_op_;
    repeat_op.repetitions = current_op_repetitions_left_;
    startOperationCritical(repeat_op);
    portEXIT_CRITICAL_ISR(&mux_);
    return;
  }

  completed = current_op_;
  current_op_ = {nullptr, 0, 0};
  current_op_repetitions_left_ = 0;

  has_next = pending_ops_.read(next);
  if (has_next) {
    startOperationCritical(next);
  } else {
    transfer_in_progress_ = false;
    uint8_t spi_port = hostToSpiPort(host_id_);
    SpiDmaTxDisable(spi_port);
    SpiDmaTransferDoneIntDisable(spi_port);
    notify_waiter = true;
  }

  if (notify_waiter && waiter_task_ != nullptr) {
    waiter = waiter_task_;
    waiter_task_ = nullptr;
  }
  portEXIT_CRITICAL_ISR(&mux_);

  if (completed.out_data != nullptr) {
    dma_buffer_pool_.release(
        DmaBufferPool::Buffer{const_cast<roo::byte*>(completed.out_data)});
  }

  if (waiter != nullptr) {
    BaseType_t high_wakeup = pdFALSE;
    vTaskNotifyGiveFromISR(waiter, &high_wakeup);
    portYIELD_FROM_ISR(high_wakeup);
  }
#endif
}

#if ROO_DISPLAY_HAS_SPI_INTERNAL_DMA
bool DmaController::startOperationCritical(Operation op) {
  CHECK_NOTNULL(dma_ctx_);
  CHECK_NOTNULL(op.out_data);
  CHECK_GT(op.out_len, 0);
  CHECK_GT(op.repetitions, 0);

  spicommon_dma_desc_setup_link(dma_ctx_->dmadesc_tx, op.out_data,
                                static_cast<int>(op.out_len), false);

#if SOC_GDMA_SUPPORTED
  gdma_reset(dma_ctx_->tx_dma_chan);
  gdma_start(dma_ctx_->tx_dma_chan,
             reinterpret_cast<intptr_t>(
                 const_cast<spi_dma_desc_t*>(dma_ctx_->dmadesc_tx)));
#else
  spi_dma_reset(dma_ctx_->tx_dma_chan);
  spi_dma_start(dma_ctx_->tx_dma_chan,
                const_cast<spi_dma_desc_t*>(dma_ctx_->dmadesc_tx));
#endif

  uint8_t spi_port = hostToSpiPort(host_id_);
  SpiSetOutBufferSize(spi_port, op.out_len);
  SpiDmaTransferDoneIntClear(spi_port);
  SpiDmaTransferDoneIntEnable(spi_port);
  SpiDmaTxEnable(spi_port);

  SpiTxStart(spi_port);
  current_op_ = op;
  current_op_repetitions_left_ = op.repetitions;
  transfer_in_progress_ = true;
  return true;
}
#endif

namespace {

struct InitializedDmaBufferPool {
  DmaBufferPool pool_;

  InitializedDmaBufferPool() : pool_() { pool_.begin(); }

  DmaBufferPool& get() { return pool_; }
};

DmaBufferPool& GetDmaBufferPool() {
  static InitializedDmaBufferPool initialized_pool;
  return initialized_pool.get();
}

}  // namespace

DmaController* GetDmaControllerForHost(int spi_port) {
  constexpr int kControllerCount = 4;
  if (spi_port <= 0 || spi_port >= kControllerCount) {
    return nullptr;
  }
  static DmaController* controllers[kControllerCount];
  if (controllers[spi_port] == nullptr) {
    // controllers[0] is unused since SPI ports are 1-indexed.
    controllers[spi_port] = new DmaController(
        static_cast<spi_host_device_t>(spi_port - 1), GetDmaBufferPool());
    controllers[spi_port]->begin();
  }
  return controllers[spi_port];
}

}  // namespace esp32
}  // namespace roo_display

#endif  // defined(ESP_PLATFORM) && !defined(ROO_TESTING)
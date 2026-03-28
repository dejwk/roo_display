#if defined(ESP_PLATFORM) && !defined(ROO_TESTING)

#include "roo_display/hal/esp32/spi_irq.h"

#include "esp_private/spi_common_internal.h"
#include "roo_display/hal/esp32/spi_reg.h"

namespace roo_display {
namespace esp32 {

void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR InterruptHandler(void* arg) {
  IrqDispatcher* dispatcher = static_cast<IrqDispatcher*>(arg);
  dispatcher->dispatch();
}

IrqDispatcher::IrqDispatcher()
    : spi_port_(0),
      init_attempted_(false),
      intr_handle_(nullptr),
      binding_(nullptr),
      mux_(portMUX_INITIALIZER_UNLOCKED) {}

bool IrqDispatcher::init(int spi_port) {
  if (intr_handle_ != nullptr) return true;
  if (init_attempted_) return false;
  init_attempted_ = true;
  spi_port_ = spi_port;
  int flags = 0;
#if ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM
  flags |= ESP_INTR_FLAG_IRAM;
#endif
#if ROO_DISPLAY_ESP32_SPI_SHARED_IRQ
  flags |= ESP_INTR_FLAG_SHARED;
#endif
  esp_err_t intr_err =
      esp_intr_alloc(spicommon_irqsource_for_host(
                         static_cast<spi_host_device_t>(spi_port - 1)),
                     flags, InterruptHandler, this, &intr_handle_);
  if (intr_err != ESP_OK) {
    log_e("Failed to allocate SPI interrupt: %s", esp_err_to_name(intr_err));
    return false;
  }
  return true;
}

bool IrqDispatcher::bind(const Binding* binding) {
  if (intr_handle_ == nullptr) return false;
  if (binding == nullptr || binding->handler == nullptr) return false;
  portENTER_CRITICAL(&mux_);
  const volatile Binding* active = binding_;
  if (active != nullptr) {
    // Idempotent bind for the same binding pointer.
    if (active == binding) {
      portEXIT_CRITICAL(&mux_);
      return true;
    }
    portEXIT_CRITICAL(&mux_);
    return false;
  }
  binding_ = binding;
  portEXIT_CRITICAL(&mux_);
  return true;
};

void IrqDispatcher::unbind(const Binding* binding) {
  if (binding == nullptr) return;
  portENTER_CRITICAL(&mux_);
  if (binding_ == binding) {
    binding_ = nullptr;
  }
  portEXIT_CRITICAL(&mux_);
}

inline void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR IrqDispatcher::dispatch() {
#if ROO_DISPLAY_ESP32_SPI_SHARED_IRQ
  if (!SpiNonDmaTransferDoneIntPending(spi_port_)) {
    // Shared IRQ line: ignore unrelated interrupt sources.
    return;
  }
#endif
  SpiNonDmaTransferDoneIntClear(spi_port_);
  const volatile Binding* binding = binding_;
  if (binding != nullptr) {
    binding->handler(binding->arg);
  }
}

IrqDispatcher* GetIrqDispatcher(int spi_port) {
  constexpr int kDispatcherCount = 4;
  if (spi_port <= 0 || spi_port >= kDispatcherCount) {
    return nullptr;
  }
  static IrqDispatcher dispatchers[4];
  IrqDispatcher& d = dispatchers[spi_port];
  if (!d.init_attempted()) {
    d.init(spi_port);
  }

  return d.ok() ? &d : nullptr;
}

}  // namespace esp32
}  // namespace roo_display

#endif  // defined(ESP_PLATFORM) && !defined(ROO_TESTING)

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
  // Start DISABLED; bind()/unbind() toggle the handler on demand so that
  // stale peripheral status bits (forced by spi_hal_init inside the master
  // driver) cannot cause an infinite ISR loop.
  int flags = ESP_INTR_FLAG_INTRDISABLED;
#if !defined(ARDUINO) || ROO_DISPLAY_ESP32_SPI_SHARED_IRQ
  // Under ESP-IDF, use SHARED so we coexist with the SPI master driver,
  // which already holds an interrupt for this source (from
  // spi_bus_add_device).  Under Arduino, the SPI library uses polling
  // (SPI.transfer), so we can keep the faster exclusive handler.
  flags |= ESP_INTR_FLAG_SHARED;
#endif
#if ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM
  flags |= ESP_INTR_FLAG_IRAM;
#endif
  spi_host_device_t host = static_cast<spi_host_device_t>(spi_port - 1);
  int irq_source = spicommon_irqsource_for_host(host);
  esp_err_t intr_err =
      esp_intr_alloc(irq_source, flags, InterruptHandler, this, &intr_handle_);
  if (intr_err != ESP_OK) {
    LOG(ERROR) << "Failed to allocate SPI interrupt: " << esp_err_to_name(intr_err);
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
  // Clear any stale transfer-done status before enabling our handler,
  // so we don't immediately fire for a leftover event.
  SpiTransferDoneIntClear(spi_port_);
  esp_intr_enable(intr_handle_);
  portEXIT_CRITICAL(&mux_);
  return true;
};

void IrqDispatcher::unbind(const Binding* binding) {
  if (binding == nullptr) return;
  portENTER_CRITICAL(&mux_);
  if (binding_ == binding) {
    esp_intr_disable(intr_handle_);
    binding_ = nullptr;
  }
  portEXIT_CRITICAL(&mux_);
}

inline void IrqDispatcher::dispatch() {
#if !defined(ARDUINO) || ROO_DISPLAY_ESP32_SPI_SHARED_IRQ
  if (!SpiTransferDoneIntPending(spi_port_)) return;
#endif
  SpiTransferDoneIntClear(spi_port_);
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
  static IrqDispatcher dispatchers[4] = {};
  IrqDispatcher& d = dispatchers[spi_port];
  if (!d.init_attempted()) {
    d.init(spi_port);
  }

  return d.ok() ? &d : nullptr;
}

}  // namespace esp32
}  // namespace roo_display

#endif  // defined(ESP_PLATFORM) && !defined(ROO_TESTING)

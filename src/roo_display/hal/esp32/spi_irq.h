#pragma once

#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "roo_logging.h"

#ifndef ROO_DISPLAY_ESP32_SPI_SHARED_IRQ
#define ROO_DISPLAY_ESP32_SPI_SHARED_IRQ 0
#endif

#ifndef ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM
#define ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM 1
#endif

#if ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM
#define ROO_DISPLAY_SPI_ASYNC_ISR_ATTR IRAM_ATTR
#else
#define ROO_DISPLAY_SPI_ASYNC_ISR_ATTR
#endif

namespace roo_display {
namespace esp32 {

class IrqDispatcher {
 public:
  struct Binding {
    intr_handler_t handler = nullptr;
    void* arg = nullptr;
  };

  bool bind(const Binding* binding);

  // Releases the currently attached callback binding for this host interrupt
  // source.
  // The underlying IRQ allocation remains active for fast future reuse.
  void unbind(const Binding* binding);

 private:
  friend void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR InterruptHandler(void* arg);
  friend IrqDispatcher* GetIrqDispatcher(int spi_port);

  IrqDispatcher();
  bool init(int spi_port);
  bool init_attempted() const { return init_attempted_; }
  bool ok() const { return intr_handle_ != nullptr; }
  int spi_port() const { return spi_port_; }

  ROO_DISPLAY_SPI_ASYNC_ISR_ATTR void dispatch();

  int spi_port_;
  bool init_attempted_;
  intr_handle_t intr_handle_;
  volatile const Binding* binding_;
  portMUX_TYPE mux_;
};

IrqDispatcher* GetIrqDispatcher(int spi_port);

}  // namespace esp32
}  // namespace roo_display

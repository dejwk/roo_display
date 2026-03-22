#include "roo_display/hal/esp32/spi_dma.h"

namespace roo_display {
namespace esp32 {

DmaController::DmaController(DmaBufferPool& dma_buffer_pool)
    : dma_buffer_pool_(dma_buffer_pool),
      pending_ops_(dma_buffer_pool.capacity()) {}


      
}  // namespace esp32
}  // namespace roo_display
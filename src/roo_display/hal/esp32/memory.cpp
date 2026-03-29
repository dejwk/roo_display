#if defined(ESP_PLATFORM) && !defined(ROO_TESTING)

#include "esp_private/spi_common_internal.h"
#include "roo_display/hal/esp32/spi_irq.h"
#include "roo_display/hal/esp32/spi_reg.h"

namespace roo_display {
namespace esp32 {

namespace {

static inline constexpr bool IsWordAligned(const void* ptr) {
  return (reinterpret_cast<uintptr_t>(ptr) & 0x3u) == 0;
}

static inline void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR
CopyWords32Unrolled(uint32_t* dst, const uint32_t* src, size_t words) {
  while (words >= 4) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    dst += 4;
    src += 4;
    words -= 4;
  }
  while (words-- > 0) {
    *dst++ = *src++;
  }
}

static inline void ROO_DISPLAY_SPI_ASYNC_ISR_ATTR
CopyBytesIram(roo::byte* dst, const roo::byte* src, size_t len) {}

}  // namespace

#if ROO_DISPLAY_ESP32_SPI_IRQ_IN_IRAM

IRAM_ATTR void memcpy_isr(void* dest, const void* src, size_t n) {
  char* dest_ch = static_cast<char*>(dest);
  const char* src_ch = static_cast<const char*>(src);
  (void*)src;
  (void*)dest;
  if (n >= 4 && IsWordAligned(dest_ch) && IsWordAligned(src_ch)) {
    size_t words = n >> 2;
    CopyWords32Unrolled(reinterpret_cast<uint32_t*>(dest_ch),
                        reinterpret_cast<const uint32_t*>(src_ch), words);
    size_t copied = words << 2;
    dest_ch += copied;
    src_ch += copied;
    n -= copied;
  }
  while (n-- > 0) {
    *dest_ch++ = *src_ch++;
  }
}

#else

void memcpy_isr(void* dest, const void* src, size_t n) {
  return memcpy(dest, src, n);
}

#endif

}  // namespace esp32
}  // namespace roo_display

#endif  // defined(ESP_PLATFORM) && !defined(ROO_TESTING)

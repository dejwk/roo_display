#include <type_traits>

#include "gtest/gtest.h"
#include "roo_display/driver/TFT_eSPI_adapter.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/ili9486.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/driver/ssd1327.h"
#include "roo_display/driver/st7735.h"
#include "roo_display/driver/st7789.h"
#include "roo_display/driver/st7796s.h"
#include "roo_display/driver/st77xx.h"
#include "roo_display/driver/touch_ft6x36.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/driver/touch_xpt2046.h"

namespace {

template <typename T>
constexpr bool IsComplete() {
  return sizeof(T) > 0;
}

TEST(DriverCompileTest, InstantiateDriverTemplates) {
  using Ili9341Spi = roo_display::Ili9341spi<1, 2, 3>;
  using Ili9486Spi = roo_display::Ili9486spi<1, 2, 3>;
  using Ili9488Spi = roo_display::Ili9488spi<1, 2, 3>;
  using Ssd1327Spi = roo_display::Ssd1327spi<1, 2, 3>;

  using St7735_128x160 = roo_display::St7735spi_128x160<1, 2, 3>;
  using St7735_80x160_inv = roo_display::St7735spi_80x160_inv<1, 2, 3>;
  using St7735_128x128 = roo_display::St7735spi_128x128<1, 2, 3>;

  using St7789_240x240 = roo_display::St7789spi_240x240<1, 2, 3>;
  using St7789_240x280 = roo_display::St7789spi_240x280<1, 2, 3>;
  using St7789_172x320 = roo_display::St7789spi_172x320<1, 2, 3>;
  using St7789_135x240 = roo_display::St7789spi_135x240<1, 2, 3>;

  using St7796sSpi = roo_display::St7796sspi<1, 2, 3>;

  using TouchFt6x36 = roo_display::TouchFt6x36;
  using TouchGt911 = roo_display::TouchGt911;
  using TouchXpt2046 = roo_display::TouchXpt2046<4>;

  static_assert(IsComplete<Ili9341Spi>());
  static_assert(IsComplete<Ili9486Spi>());
  static_assert(IsComplete<Ili9488Spi>());
  static_assert(IsComplete<Ssd1327Spi>());

  static_assert(IsComplete<St7735_128x160>());
  static_assert(IsComplete<St7735_80x160_inv>());
  static_assert(IsComplete<St7735_128x128>());

  static_assert(IsComplete<St7789_240x240>());
  static_assert(IsComplete<St7789_240x280>());
  static_assert(IsComplete<St7789_172x320>());
  static_assert(IsComplete<St7789_135x240>());

  static_assert(IsComplete<St7796sSpi>());

  static_assert(IsComplete<TouchFt6x36>());
  static_assert(IsComplete<TouchGt911>());
  static_assert(IsComplete<TouchXpt2046>());

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3)
  static_assert(IsComplete<roo_display::esp32s3_dma::ParallelRgb565Buffered>());
#endif

#ifdef TFT_ESPI_VERSION
  static_assert(IsComplete<roo_display::TFT_eSPI_Adapter>());
#endif

  SUCCEED();
}

}  // namespace

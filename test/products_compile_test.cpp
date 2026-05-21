#include <type_traits>

#include "gtest/gtest.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/products/noname/ili9341_red/kmrtm32032_spi.h"
#include "roo_display/products/noname/ili9341_red/tjctm24024_spi.h"
#include "roo_display/products/noname/st7735_red/tft_module_128x128_144_in.h"
#include "roo_display/products/noname/st7796s_black/msp4031.h"
#include "roo_display/products/waveshare/lcd_module_128x160_18_in.h"
#include "roo_display/products/waveshare/lcd_module_160x80_096_in.h"
#include "roo_display/products/waveshare/tft_touch_shield_4in.h"

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32)
#include "roo_display/products/makerfabs/esp32_tft_capacitive_35.h"
#endif

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include "roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive.h"
#include "roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive_43.h"
#include "roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive_70.h"
#endif

#if defined(ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include "roo_display/products/lilygo/t_display_s3.h"
#include "roo_display/products/waveshare/waveshare-esp32-s3-touch-lcd-43/waveshare_esp32s3_touch_lcd_43.h"
#endif

namespace {

template <typename T>
constexpr bool IsComplete() {
  return sizeof(T) > 0;
}

TEST(ProductsCompileTest, InstantiateProductTypes) {
  using LcdModule160x80 =
      roo_display::products::waveshare::LcdModule_160x80<1, 2, 3>;
  using LcdModule128x160 =
      roo_display::products::waveshare::LcdModule_128x160<1, 2, 3>;
  using TftTouchShield4in =
      roo_display::products::waveshare::TftTouchShield4in<1, 2, 3, 4, 5>;

  using Kmrtm32032 =
      roo_display::products::noname::ili9341_red::Kmrtm32032Spi<1, 2, 3>;
  using Tjctm24024 =
      roo_display::products::noname::ili9341_red::Tjctm24024Spi<1, 2, 3>;
  using TftModule128x128 =
      roo_display::products::noname::st7735_red::TftModule_128x128_144in<1, 2,
                                                                         3>;
  using Msp4031 =
      roo_display::products::noname::st7796s_black::Msp4031<1, 2, 3>;

  static_assert(IsComplete<LcdModule160x80>());
  static_assert(IsComplete<LcdModule128x160>());
  static_assert(IsComplete<TftTouchShield4in>());

  static_assert(IsComplete<Kmrtm32032>());
  static_assert(IsComplete<Tjctm24024>());
  static_assert(IsComplete<TftModule128x128>());
  static_assert(IsComplete<Msp4031>());

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32)
  static_assert(
      IsComplete<roo_display::products::makerfabs::Esp32TftCapacitive35>());
#endif

#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3)
  static_assert(
      IsComplete<
          roo_display::products::makerfabs::Esp32s3ParallelIpsCapacitive>());
  static_assert(IsComplete<roo_display::products::makerfabs::
                               Esp32s3ParallelIpsCapacitive800x480>());
  static_assert(IsComplete<roo_display::products::makerfabs::
                               Esp32s3ParallelIpsCapacitive1024x600>());
  static_assert(
      IsComplete<
          roo_display::products::makerfabs::Esp32s3ParallelIpsCapacitive43>());
  static_assert(
      IsComplete<
          roo_display::products::makerfabs::Esp32s3ParallelIpsCapacitive70>());
#endif

#if defined(ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
  static_assert(IsComplete<roo_display::products::lilygo::TDisplayS3>());
  static_assert(
      IsComplete<
          roo_display::products::waveshare::WaveshareEsp32s3TouchLcd43>());
#endif

  SUCCEED();
}

TEST(ProductsCompileTest, SmoothRoundRectCornerRadiiApiCompiles) {
    static_assert(std::is_aggregate<roo_display::RoundRectRadii>::value,
                                "RoundRectRadii should stay an aggregate");

    const roo_display::RoundRectRadii radii{2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_FLOAT_EQ(2.0f, radii.tl);
    EXPECT_FLOAT_EQ(5.0f, radii.br);

    const roo_display::SmoothShape outline = roo_display::SmoothRoundRect(
            0.0f, 0.0f, 10.0f, 12.0f, radii, roo_display::color::Black);
    const roo_display::SmoothShape thick = roo_display::SmoothThickRoundRect(
            0.0f, 0.0f, 10.0f, 12.0f, radii, 2.0f, roo_display::color::Black,
            roo_display::color::Transparent);
    const roo_display::SmoothShape filled = roo_display::SmoothFilledRoundRect(
            0.0f, 0.0f, 10.0f, 12.0f, radii, roo_display::color::Black);

    (void)outline;
    (void)thick;
    (void)filled;
    SUCCEED();
}

}  // namespace

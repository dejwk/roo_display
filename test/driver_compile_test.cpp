#include <cstdint>
#include <initializer_list>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "roo_display/driver/TFT_eSPI_adapter.h"
#if defined(ESP_PLATFORM) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#endif
#include "roo_display/driver/gc9a01a.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/ili9486.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/driver/ssd1327.h"
#include "roo_display/driver/st7701.h"
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
  using Gc9a01aSpi = roo_display::Gc9a01aspi_240x240<1, 2, 3>;
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

  static_assert(IsComplete<Gc9a01aSpi>());
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

// Verifies bank-specific ST7701S command IDs retain their datasheet values,
// including addresses reused by different banks in the Makerfabs sequence.
TEST(DriverCompileTest, St7701CommandIds) {
  using namespace roo_display::st7701;

  EXPECT_EQ(CMD2BKXSEL, 0xFF);
  EXPECT_EQ(BK0_PVGAMCTRL, 0xB0);
  EXPECT_EQ(BK0_NVGAMCTRL, 0xB1);
  EXPECT_EQ(BK0_LNESET, 0xC0);
  EXPECT_EQ(BK0_PORCTRL, 0xC1);
  EXPECT_EQ(BK0_INVSET, 0xC2);
  EXPECT_EQ(BK0_COLCTRL, 0xCD);

  EXPECT_EQ(BK1_VRHS, 0xB0);
  EXPECT_EQ(BK1_VCOMS, 0xB1);
  EXPECT_EQ(BK1_VGHSS, 0xB2);
  EXPECT_EQ(BK1_TESTCMD, 0xB3);
  EXPECT_EQ(BK1_VGLS, 0xB5);
  EXPECT_EQ(BK1_PWCTRL1, 0xB7);
  EXPECT_EQ(BK1_PWCTRL2, 0xB8);
  EXPECT_EQ(BK1_SPD1, 0xC1);
  EXPECT_EQ(BK1_SPD2, 0xC2);

  EXPECT_EQ(BK1_GIP_E0, 0xE0);
  EXPECT_EQ(BK1_GIP_E1, 0xE1);
  EXPECT_EQ(BK1_GIP_E5, 0xE5);
  EXPECT_EQ(BK1_GIP_ED, 0xED);
  EXPECT_EQ(BK3_E5, 0xE5);

  EXPECT_EQ(SWRESET, 0x01);
  EXPECT_EQ(SLPOUT, 0x11);
  EXPECT_EQ(INVON, 0x21);
  EXPECT_EQ(DISPON, 0x29);
  EXPECT_EQ(MADCTL, 0x36);
  EXPECT_EQ(COLMOD, 0x3A);
}

struct RecordedSt7701Command {
  uint8_t command;
  std::vector<uint8_t> parameters;
  uint32_t delay_ms;
};

struct RecordingSt7701Target {
  std::vector<RecordedSt7701Command> commands;

  void writeCommand(roo_display::st7701::Command command,
                    std::initializer_list<uint8_t> parameters = {},
                    uint32_t delay_ms = 0) {
    commands.push_back({command, std::vector<uint8_t>(parameters), delay_ms});
  }
};

// Verifies reset and sleep-out delays, bank selection, panel timing, and the
// RGB color order used by the Makerfabs startup sequence.
TEST(DriverCompileTest, St7701StartupSequence) {
  RecordingSt7701Target target;
  roo_display::st7701::Init().init(target);

  ASSERT_EQ(target.commands.size(), 37U);
  EXPECT_EQ(target.commands[0].command, 0x01);
  EXPECT_EQ(target.commands[0].delay_ms, 120U);
  EXPECT_EQ(target.commands[1].command, 0xFF);
  EXPECT_EQ(target.commands[1].parameters.back(), 0x10);
  EXPECT_EQ(target.commands[3].command, 0xC1);
  EXPECT_EQ(target.commands[3].parameters, std::vector<uint8_t>({0x0D, 0x02}));
  EXPECT_EQ(target.commands[4].command, 0xC2);
  EXPECT_EQ(target.commands[4].parameters, std::vector<uint8_t>({0x31, 0x05}));
  EXPECT_EQ(target.commands[8].command, 0xFF);
  EXPECT_EQ(target.commands[8].parameters.back(), 0x11);
  EXPECT_EQ(target.commands[19].command, 0xE1);
  EXPECT_EQ(target.commands[19].parameters.size(), 11U);
  EXPECT_EQ(target.commands[30].command, 0xFF);
  EXPECT_EQ(target.commands[30].parameters.back(), 0x13);
  EXPECT_EQ(target.commands[31].command, 0xE5);
  EXPECT_EQ(target.commands[32].command, 0xFF);
  EXPECT_EQ(target.commands[32].parameters.back(), 0x00);
  EXPECT_EQ(target.commands[33].command, 0x21);
  EXPECT_EQ(target.commands[34].command, 0x3A);
  EXPECT_EQ(target.commands[34].parameters, std::vector<uint8_t>({0x60}));
  EXPECT_EQ(target.commands[35].command, 0x11);
  EXPECT_EQ(target.commands[35].delay_ms, 120U);
  EXPECT_EQ(target.commands[36].command, 0x29);
}

}  // namespace

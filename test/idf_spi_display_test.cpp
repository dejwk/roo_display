#include "gtest/gtest.h"
#include "roo_display/color/color.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/named.h"
#include "roo_display/driver/ili9341.h"
#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "testing_viewport.h"

#ifndef ESP_PLATFORM
#error "This test requires the roo_testing ESP-IDF profile"
#endif

#ifdef ARDUINO
#error "This is an ESP-IDF-only test; ARDUINO must not be defined"
#endif

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 5;
constexpr int kPinDc = 16;
constexpr int kPinRst = 4;

uint32_t ToRgb565Argb(roo_display::Color color) {
  roo_display::Rgb565 mode;
  return mode.toArgbColor(mode.fromArgbColor(color)).asArgb();
}

struct IdfIli9341Emulator {
  testing::TestViewport viewport;
  FakeIli9341Spi display;

  IdfIli9341Emulator() : viewport(), display(viewport) {
    FakeEsp32().attachSpiDevice(display, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, display.cs());
    FakeEsp32().gpio.attachOutput(kPinDc, display.dc());
    FakeEsp32().gpio.attachOutput(kPinRst, display.rst());
  }
};

TEST(EspIdfSpiDisplay, InitializesAndWritesAPixel) {
  // FakeEsp32 keeps device attachment pointers for the process lifetime.
  static IdfIli9341Emulator emulator;

  roo_display::DefaultSpi spi;
  spi.init(kSpiSck, kSpiMiso, kSpiMosi);
  {
    roo_display::Ili9341spi<kPinCs, kPinDc, kPinRst> panel(spi);
    EXPECT_EQ(240, panel.raw_width());
    EXPECT_EQ(320, panel.raw_height());

    panel.init();
    panel.begin();
    panel.setAddress(0, 0, 0, 0, roo_display::BlendingMode::kSource);
    panel.fill(roo_display::color::Red, 1);
    panel.end();
    FakeEsp32().flush();

    EXPECT_EQ(ToRgb565Argb(roo_display::color::Red),
              emulator.viewport.getPixel(0, 0));
    EXPECT_EQ(ToRgb565Argb(roo_display::color::Black),
              emulator.viewport.getPixel(1, 0));
  }
  spi.deinit();
  spi.deinit();
}

}  // namespace

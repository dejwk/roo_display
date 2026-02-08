#include <Arduino.h>

#include "SPI.h"
#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic.h"
#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 5;
constexpr int kPinDc = 2;
constexpr int kPinRst = 4;

uint32_t ToRgb565Argb(Color color) {
  Rgb565 mode;
  return mode.toArgbColor(mode.fromArgbColor(color)).asArgb();
}

struct Ili9341Emulator {
  testing::TestViewport viewport;
  FakeIli9341Spi display;

  Ili9341Emulator() : viewport(), display(viewport) {
    FakeEsp32().attachSpiDevice(display, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, display.cs());
    FakeEsp32().gpio.attachOutput(kPinDc, display.dc());
    FakeEsp32().gpio.attachOutput(kPinRst, display.rst());
  }
};

}  // namespace

TEST(Ili9341Driver, DrawFilledRect) {
  Ili9341Emulator emu;

  Ili9341spi<kPinCs, kPinDc, kPinRst> device;
  Display display(device);

  SPI.begin();
  display.init(color::Black);

  {
    DrawingContext dc(display);
    dc.draw(FilledRect(10, 20, 12, 21, color::Red));
  }

  FakeEsp32().flush();

  EXPECT_EQ(ToRgb565Argb(color::Red), emu.viewport.getPixel(10, 20));
  EXPECT_EQ(ToRgb565Argb(color::Black), emu.viewport.getPixel(0, 0));
}

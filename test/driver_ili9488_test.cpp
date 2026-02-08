#include <Arduino.h>

#include "SPI.h"
#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/driver/ili9488.h"
#include "roo_display/shape/basic.h"
#include "roo_testing/devices/display/ili9488/ili9488spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 16;
constexpr int kPinDc = 17;
constexpr int kPinRst = 27;

uint32_t ToRgb666hArgb(Color color) {
  // Round-trip through the device color mode to simulate on-device
  // truncation before comparing against emulator output.
  ili9488::Rgb666h mode;
  return mode.toArgbColor(mode.fromArgbColor(color)).asArgb();
}

struct Ili9488Emulator {
  testing::TestViewport viewport;
  FakeIli9488Spi display;

  Ili9488Emulator() : viewport(), display(viewport) {
    FakeEsp32().attachSpiDevice(display, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, display.cs());
    FakeEsp32().gpio.attachOutput(kPinDc, display.dc());
    FakeEsp32().gpio.attachOutput(kPinRst, display.rst());
  }
};

}  // namespace

TEST(Ili9488Driver, DrawFilledRect) {
  Ili9488Emulator emu;

  Ili9488spi<kPinCs, kPinDc, kPinRst> device;
  Display display(device);

  SPI.begin();
  display.init(color::Black);

  {
    DrawingContext dc(display);
    dc.draw(FilledRect(8, 9, 10, 11, color::Red));
    dc.draw(FilledRect(2, 3, 2, 3, color::Gray));
    dc.draw(FilledRect(4, 5, 4, 5, color::DarkGray));
    dc.draw(FilledRect(6, 7, 6, 7, color::DeepSkyBlue));
  }

  FakeEsp32().flush();

  EXPECT_EQ(ToRgb666hArgb(color::Red), emu.viewport.getPixel(8, 9));
  EXPECT_EQ(ToRgb666hArgb(color::Black), emu.viewport.getPixel(0, 0));
  EXPECT_EQ(ToRgb666hArgb(color::Gray), emu.viewport.getPixel(2, 3));
  EXPECT_EQ(ToRgb666hArgb(color::DarkGray), emu.viewport.getPixel(4, 5));
  EXPECT_EQ(ToRgb666hArgb(color::DeepSkyBlue), emu.viewport.getPixel(6, 7));
}

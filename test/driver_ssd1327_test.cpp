#include <Arduino.h>

#include "SPI.h"
#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/pixel_order.h"
#include "roo_display/driver/ssd1327.h"
#include "roo_display/shape/basic.h"
#include "roo_testing/devices/display/ssd1327/ssd1327spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 26;
constexpr int kPinDc = 33;
constexpr int kPinRst = 32;

uint32_t ToGrayscale4Viewport(Color color) {
  // Round-trip through the device color mode to simulate on-device
  // truncation before comparing against emulator output.
  Grayscale4 mode;
  uint8_t raw = mode.fromArgbColor(color);
  return 0xFF000000 | static_cast<uint32_t>(raw) * 0x00111111;
}

struct Ssd1327Emulator {
  testing::TestViewport viewport;
  FakeSsd1327Spi display;

  Ssd1327Emulator() : viewport(), display(viewport) {
    FakeEsp32().attachSpiDevice(display, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, display.cs());
    FakeEsp32().gpio.attachOutput(kPinDc, display.dc());
    FakeEsp32().gpio.attachOutput(kPinRst, display.rst());
  }
};

}  // namespace

TEST(Ssd1327Driver, DrawFilledRect) {
  Ssd1327Emulator emu;

  Ssd1327spi<kPinCs, kPinDc, kPinRst> device;
  Display display(device);

  SPI.begin();
  display.init(color::Black);

  {
    DrawingContext dc(display);
    dc.draw(FilledRect(1, 1, 2, 2, color::White));
    dc.draw(FilledRect(3, 3, 3, 3, color::Gray));
    dc.draw(FilledRect(4, 4, 4, 4, color::DarkGray));
    dc.draw(FilledRect(5, 5, 5, 5, color::Orange));
  }

  FakeEsp32().flush();

  EXPECT_EQ(ToGrayscale4Viewport(color::White), emu.viewport.getPixel(1, 1));
  EXPECT_EQ(ToGrayscale4Viewport(color::Black), emu.viewport.getPixel(0, 0));
  EXPECT_EQ(ToGrayscale4Viewport(color::Gray), emu.viewport.getPixel(3, 3));
  EXPECT_EQ(ToGrayscale4Viewport(color::DarkGray), emu.viewport.getPixel(4, 4));
  EXPECT_EQ(ToGrayscale4Viewport(color::Orange), emu.viewport.getPixel(5, 5));
}

TEST(Ssd1327Driver, ColorFormat) {
  Ssd1327Emulator emu;

  Ssd1327spi<kPinCs, kPinDc, kPinRst> device;
  const auto& format = device.getColorFormat();

  EXPECT_EQ(DisplayOutput::ColorFormat::kModeGrayscale4, format.mode());
  EXPECT_EQ(roo_io::kBigEndian, format.byte_order());
  EXPECT_EQ(COLOR_PIXEL_ORDER_MSB_FIRST, format.pixel_order());
}

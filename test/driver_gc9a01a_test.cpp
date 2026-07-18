#include <Arduino.h>

#include "SPI.h"
#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/pixel_order.h"
#include "roo_display/driver/gc9a01a.h"
#include "roo_display/shape/basic.h"
#include "roo_testing/devices/display/st77xx/st77xx.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 21;
constexpr int kPinDc = 22;
constexpr int kPinRst = 25;

uint32_t ToRgb565Argb(Color color) {
  Rgb565 mode;
  return mode.toArgbColor(mode.fromArgbColor(color)).asArgb();
}

struct Gc9a01aEmulator {
  testing::TestViewport viewport;
  FakeSt77xxSpi display;

  Gc9a01aEmulator() : viewport(), display(viewport, 240, 240) {
    FakeEsp32().attachSpiDevice(display, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, display.cs());
    FakeEsp32().gpio.attachOutput(kPinDc, display.dc());
    FakeEsp32().gpio.attachOutput(kPinRst, display.rst());
  }
};

static Gc9a01aEmulator emu;

}  // namespace

TEST(Gc9a01aDriver, DrawFilledRect) {
  Gc9a01aspi_240x240<kPinCs, kPinDc, kPinRst> device;
  Display display(device);

  SPI.begin();
  display.init(color::Black);

  {
    DrawingContext dc(display);
    dc.draw(FilledRect(3, 4, 5, 6, color::Green));
  }

  FakeEsp32().flush();

  EXPECT_EQ(ToRgb565Argb(color::Green), emu.viewport.getPixel(5, 5));
  EXPECT_EQ(ToRgb565Argb(color::Black), emu.viewport.getPixel(0, 0));
}

TEST(Gc9a01aDriver, ColorFormat) {
  Gc9a01aspi_240x240<kPinCs, kPinDc, kPinRst> device;
  const auto& format = device.getColorFormat();

  EXPECT_EQ(DisplayOutput::ColorFormat::kModeRgb565, format.mode());
  EXPECT_EQ(roo_io::kBigEndian, format.byte_order());
  EXPECT_EQ(ColorPixelOrder::kMsbFirst, format.pixel_order());
}

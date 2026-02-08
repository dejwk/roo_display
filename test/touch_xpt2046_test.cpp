#include <Arduino.h>

#include "SPI.h"
#include "gtest/gtest.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/system/timer.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 15;

int16_t ScaleToRaw(int16_t value, int16_t max) {
  return static_cast<int16_t>((4096LL * value) / max);
}

struct TouchEmulator {
  testing::TestViewport viewport;
  FakeXpt2046Spi touch;

  TouchEmulator() : viewport(), touch(viewport) {
    viewport.init(240, 320);
    FakeEsp32().attachSpiDevice(touch, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, touch.cs());
  }
};

}  // namespace

TEST(TouchXpt2046, ReportsTouchCoordinates) {
  TouchEmulator emu;

  TouchXpt2046<kPinCs> touch;
  SPI.begin();
  touch.initTouch();

  emu.viewport.setMouse(20, 30, true);
  system_time_delay_micros(10000);

  TouchPoint tp;
  TouchResult result = touch.getTouch(&tp, 1);

  EXPECT_EQ(1, result.touch_points);
  EXPECT_EQ(ScaleToRaw(20, emu.viewport.width()), tp.x);
  EXPECT_EQ(ScaleToRaw(30, emu.viewport.height()), tp.y);
}

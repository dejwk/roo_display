#include "roo_display/driver/touch_ft6x36.h"

#include "gtest/gtest.h"
#include "roo_testing/devices/touch/ft6x36/ft6x36.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/system/timer.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kPinSda = 21;
constexpr int kPinScl = 22;

int16_t ScaleToRaw(int16_t value, int16_t max) {
  return static_cast<int16_t>((4096LL * value) / max);
}

/// Keeps the emulated touch panel connected to its viewport and bus pins.
struct TouchEmulator {
  testing::TestViewport viewport;
  FakeFt6x36 touch;

  /// Initializes the viewport and attaches the touch controller.
  TouchEmulator() : viewport(), touch(viewport) {
    viewport.init(240, 320);
    FakeEsp32().attachI2cDevice(touch, kPinSda, kPinScl);
  }
};

}  // namespace

// Verifies the shared I2C transport reports emulated touch coordinates on both
// backends.
TEST(TouchFt6x36, ReportsTouchCoordinates) {
  static TouchEmulator emu;

  roo_io::I2cMasterBusHandle bus;
  ASSERT_TRUE(bus.init(kPinSda, kPinScl));
  TouchFt6x36 touch(bus);
  touch.initTouch();

  emu.viewport.setMouse(40, 50, true);
  system_time_delay_micros(30000);

  TouchPoint tp;
  TouchResult result = touch.getTouch(&tp, 1);

  EXPECT_EQ(1, result.touch_points);
  EXPECT_EQ(ScaleToRaw(40, emu.viewport.width()), tp.x);
  EXPECT_EQ(ScaleToRaw(50, emu.viewport.height()), tp.y);
}

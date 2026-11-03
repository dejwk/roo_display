#include <vector>

#include "gtest/gtest.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "testing_viewport.h"
#if defined(ROO_TESTING_SOC_RP2040)
#include "roo_testing/microcontrollers/rp2040/fake_pico.h"
#else
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#endif

namespace {
class RecordingTouch : public FakeXpt2046Spi {
 public:
  explicit RecordingTouch(roo_testing_transducers::Viewport& viewport)
      : FakeXpt2046Spi(viewport) {}
  void transfer(const FakeSpiInterface& bus, uint8_t* data,
                uint16_t bits) override {
    EXPECT_TRUE(isSelected());
    EXPECT_EQ(bus.dataMode(), kSpiMode0);
    EXPECT_EQ(bus.bitOrder(), kSpiMsbFirst);
    bytes.insert(bytes.end(), data, data + bits / 8);
    FakeXpt2046Spi::transfer(bus, data, bits);
  }
  std::vector<uint8_t> bytes;
  int assertions = 0;

 protected:
  void onChipSelect(bool selected) override {
    if (selected) ++assertions;
  }
};

#if defined(ROO_TESTING_SOC_RP2040)
constexpr int kCs = 17;
constexpr int kMiso = 16;
constexpr int kMosi = 19;
#else
constexpr int kCs = 15;
constexpr int kMiso = 19;
constexpr int kMosi = 21;
#endif

struct Wiring {
  Wiring() {
    viewport.init(240, 320);
#if defined(ROO_TESTING_SOC_RP2040)
    FakePico().attachSpiDevice(touch, 18, kMiso, kMosi);
    FakePico().attachOutput(kCs, touch.cs());
#else
    FakeEsp32().attachSpiDevice(touch, 18, kMiso, kMosi);
    FakeEsp32().gpio.attachOutput(kCs, touch.cs());
#endif
  }
  testing::TestViewport viewport;
  RecordingTouch touch{viewport};
} wiring;

class RawTouch : public roo_display::TouchXpt2046<kCs> {
 public:
  using TouchXpt2046::TouchXpt2046;
  int sample(roo_display::TouchPoint& point) { return readTouch(&point); }
};

// Verifies the production touch driver reaches the chip model through Roo
// SPI, preserves coordinates, and closes one CS frame with PENIRQ cleanup.
TEST(Xpt2046Transport, ScopedSamplingAndRecovery) {
  static_assert(
      roo_display::internal::UseSharedTouchSpi<roo_display::DefaultSpi,
                                               roo_display::DefaultGpio>::value,
      "Standard touch buses must use roo_io");
  roo_display::DefaultSpi spi;
  spi.init(18, kMiso, kMosi);
  RawTouch touch(spi);
  roo_display::TouchPoint point;
  EXPECT_EQ(touch.sample(point), 0);
  EXPECT_EQ(wiring.touch.assertions, 0);
  touch.initTouch();
  wiring.viewport.setMouse(20, 30, true);
  ASSERT_EQ(touch.sample(point), 1);
  EXPECT_EQ(point.x, (4096LL * 20) / 240);
  EXPECT_EQ(point.y, (4096LL * 30) / 320);
  EXPECT_EQ(wiring.touch.assertions, 1);
  EXPECT_FALSE(wiring.touch.isSelected());
  ASSERT_GE(wiring.touch.bytes.size(), 3u);
  EXPECT_EQ((std::vector<uint8_t>{0x82, 0, 0}),
            std::vector<uint8_t>(wiring.touch.bytes.end() - 3,
                                 wiring.touch.bytes.end()));
  wiring.viewport.setMouse(0, 0, false);
  EXPECT_EQ(touch.sample(point), 0);
  EXPECT_FALSE(wiring.touch.isSelected());
#if defined(ARDUINO)
  SPI.end();
#else
  spi.deinit();
#endif
}
}  // namespace

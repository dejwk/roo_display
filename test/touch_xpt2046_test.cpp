#include "roo_display/driver/touch_xpt2046.h"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "roo_display/color/named.h"
#include "roo_display/driver/ili9341.h"
#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/system/timer.h"
#include "testing_viewport.h"

#if !defined(ARDUINO)
namespace {
int fail_transfer_at = -1;
int transfer_calls = 0;
std::vector<uint8_t> last_attempt;
}  // namespace
extern "C" esp_err_t __real_spi_device_polling_transmit(spi_device_handle_t,
                                                        spi_transaction_t*);
extern "C" esp_err_t __wrap_spi_device_polling_transmit(
    spi_device_handle_t device, spi_transaction_t* tx) {
  const uint8_t* data = static_cast<const uint8_t*>(tx->tx_buffer);
  if (data != nullptr) last_attempt.assign(data, data + tx->length / 8);
  if (++transfer_calls == fail_transfer_at) return ESP_FAIL;
  return __real_spi_device_polling_transmit(device, tx);
}
#endif

namespace {
using namespace roo_display;
constexpr int kPinCs = 15;
constexpr int kPanelCs = 14;
constexpr int kPanelDc = 27;
constexpr int kPanelRst = 26;

class RecordingTouch : public FakeXpt2046Spi {
 public:
  explicit RecordingTouch(roo_testing_transducers::Viewport& viewport)
      : FakeXpt2046Spi(viewport) {}

  void transfer(const FakeSpiInterface& spi, uint8_t* data,
                uint16_t bits) override {
    EXPECT_TRUE(isSelected());
    EXPECT_EQ(::kSpiMode0, spi.dataMode());
    EXPECT_EQ(::kSpiMsbFirst, spi.bitOrder());
    EXPECT_EQ(2500000u, spi.clkHz());
    ASSERT_FALSE(frames.empty());
    const size_t len = (bits + 7) / 8;
    frames.back().insert(frames.back().end(), data, data + len);
    FakeXpt2046Spi::transfer(spi, data, bits);
    if (corrupt_next) {
      corrupt_remaining_ = 5;
      corrupt_next = false;
    }
    const size_t corrupt = std::min(len, corrupt_remaining_);
    std::fill(data, data + corrupt, 0xff);
    corrupt_remaining_ -= corrupt;
  }

  void clear() {
    frames.clear();
    corrupt_next = false;
    corrupt_remaining_ = 0;
  }

  std::vector<std::vector<uint8_t>> frames;
  bool corrupt_next = false;

 private:
  size_t corrupt_remaining_ = 0;

 protected:
  void onChipSelect(bool selected) override {
    if (selected) frames.emplace_back();
  }
};

struct Hardware {
  testing::TestViewport viewport;
  testing::TestViewport panel_viewport;
  RecordingTouch touch{viewport};
  FakeIli9341Spi panel{panel_viewport};
  Hardware() {
    viewport.init(240, 320);
    panel_viewport.init(240, 320);
    FakeEsp32().attachSpiDevice(touch, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(kPinCs, touch.cs());
    FakeEsp32().attachSpiDevice(panel, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(kPanelCs, panel.cs());
    FakeEsp32().gpio.attachOutput(kPanelDc, panel.dc());
    FakeEsp32().gpio.attachOutput(kPanelRst, panel.rst());
  }
} hardware;

template <int pin = kPinCs, typename Spi = DefaultSpi,
          typename Gpio = DefaultGpio>
class RawTouch : public TouchXpt2046<pin, Spi, Gpio> {
 public:
  using TouchXpt2046<pin, Spi, Gpio>::TouchXpt2046;
  int sample(TouchPoint& point) { return this->readTouch(&point); }
};

class TouchXpt2046Test : public testing::Test {
 protected:
  void SetUp() override {
#if !defined(ARDUINO)
    fail_transfer_at = -1;
    transfer_calls = 0;
#endif
    spi_.init(18, 19, 23);
    DefaultGpio::setOutput(kPanelCs);
    DefaultGpio::setHigh<kPanelCs>();
    hardware.viewport.setMouse(20, 30, true);
    touch_.reset(new RawTouch<>(spi_));
    touch_->initTouch();
    hardware.touch.clear();
  }
  void TearDown() override {
    touch_.reset();
    EXPECT_FALSE(hardware.touch.isSelected());
#if defined(ARDUINO)
    SPI.end();
#else
    spi_.deinit();
#endif
  }
  void expectFrameCleanup() {
    ASSERT_EQ(1u, hardware.touch.frames.size());
    const std::vector<uint8_t>& frame = hardware.touch.frames[0];
    ASSERT_GE(frame.size(), 3u);
    EXPECT_EQ((std::vector<uint8_t>{0x82, 0, 0}),
              std::vector<uint8_t>(frame.end() - 3, frame.end()));
    EXPECT_FALSE(hardware.touch.isSelected());
  }
  DefaultSpi spi_;
  std::unique_ptr<RawTouch<>> touch_;
};

// Verifies real SPI adapters use one CS scope and explicit big-endian ADC
// words while preserving the established coordinate and pressure mapping.
TEST_F(TouchXpt2046Test, CoordinatesAndOneCsFrame) {
  static_assert(
      roo_display::internal::UseSharedTouchSpi<DefaultSpi, DefaultGpio>::value,
      "Default touch must use roo_io");
  TouchPoint point;
  ASSERT_EQ(1, touch_->sample(point));
  EXPECT_EQ((4096LL * 20) / 240, point.x);
  EXPECT_EQ((4096LL * 30) / 320, point.y);
  EXPECT_EQ(1250, point.z);
  expectFrameCleanup();
  const std::vector<uint8_t>& frame = hardware.touch.frames[0];
  const std::vector<uint8_t> xy = {0xd3, 0,    0xd3, 0,    0xd3, 0, 0x93,
                                   0,    0x93, 0,    0x93, 0,    0};
  EXPECT_NE(frame.end(),
            std::search(frame.begin(), frame.end(), xy.begin(), xy.end()));
}

// Verifies BasicTouchDevice still reports the existing public getTouch API.
TEST_F(TouchXpt2046Test, PublicSamplingApi) {
  system_time_delay_micros(10000);
  TouchPoint point;
  const TouchResult result = touch_->getTouch(&point, 1);
  EXPECT_EQ(1, result.touch_points);
  EXPECT_EQ((4096LL * 20) / 240, point.x);
  EXPECT_EQ((4096LL * 30) / 320, point.y);
}

// Verifies no-contact and malformed ADC data cannot produce false touches;
// cleanup happens before a subsequent valid read is attempted.
TEST_F(TouchXpt2046Test, RejectsBadDataAndRecovers) {
  hardware.viewport.setMouse(0, 0, false);
  TouchPoint point;
  EXPECT_EQ(0, touch_->sample(point));
  expectFrameCleanup();
  hardware.touch.clear();
  hardware.viewport.setMouse(20, 30, true);
  hardware.touch.corrupt_next = true;
  point.x = 123;
  EXPECT_EQ(0, touch_->sample(point));
  EXPECT_EQ(123, point.x);
  expectFrameCleanup();
  hardware.touch.clear();
  EXPECT_EQ(1, touch_->sample(point));
  expectFrameCleanup();
}

// Verifies setup is required, invalid CS fails without bus traffic, and
// existing default/explicit constructors both instantiate the shared path.
TEST_F(TouchXpt2046Test, Initialization) {
  RawTouch<> before_init;
  TouchPoint point;
  EXPECT_EQ(0, before_init.sample(point));
  RawTouch<-1> invalid(spi_);
  invalid.initTouch();
  EXPECT_EQ(0, invalid.sample(point));
  EXPECT_TRUE(hardware.touch.frames.empty());
}

// Hiding busHandle models a pre-existing third-party SPI policy. Inherited
// Device constructors continue to accept its DefaultSpi base as before.
class LegacySpi : public DefaultSpi {
 public:
  explicit LegacySpi(DefaultSpi spi) : DefaultSpi(spi) {}

 private:
  using DefaultSpi::busHandle;
};

// Verifies existing custom SPI policy types still use their old CS policy.
TEST_F(TouchXpt2046Test, CustomPolicyCompatibility) {
  static_assert(
      !roo_display::internal::UseSharedTouchSpi<LegacySpi, DefaultGpio>::value,
      "Legacy policy fallback required");
  RawTouch<kPinCs, LegacySpi> legacy{LegacySpi(spi_)};
  legacy.initTouch();
  TouchPoint point;
  EXPECT_EQ(1, legacy.sample(point));
  EXPECT_EQ((4096LL * 20) / 240, point.x);
  expectFrameCleanup();
}

// Verifies the existing panel driver and new touch scope can alternate on one
// application-owned bus, restoring settings and releasing reservations.
TEST_F(TouchXpt2046Test, SharesBusWithExistingDisplay) {
  Ili9341spi<kPanelCs, kPanelDc, kPanelRst> panel(spi_);
  panel.init();
  panel.begin();
  panel.setAddress(0, 0, 0, 0, BlendingMode::kSource);
  panel.fill(color::Red, 1);
  panel.end();
  TouchPoint point;
  ASSERT_EQ(1, touch_->sample(point));
  panel.begin();
  panel.setAddress(1, 0, 1, 0, BlendingMode::kSource);
  panel.fill(color::Blue, 1);
  panel.end();
  FakeEsp32().flush();
  EXPECT_EQ(0xffff0000u, hardware.panel_viewport.getPixel(0, 0));
  EXPECT_EQ(0xff0000ffu, hardware.panel_viewport.getPixel(1, 0));
  expectFrameCleanup();
}

#if !defined(ARDUINO)
// Verifies native transport errors reject the complete sample, still attempt
// PENIRQ cleanup under CS, release the bus, and permit the next read.
TEST_F(TouchXpt2046Test, NativeFailureCleanup) {
  TouchPoint point;
  const int before = transfer_calls;
  ASSERT_EQ(1, touch_->sample(point));
  const int calls = transfer_calls - before;
  for (int failure = 1; failure <= calls; ++failure) {
    hardware.touch.clear();
    fail_transfer_at = transfer_calls + failure;
    point.x = 123;
    EXPECT_EQ(0, touch_->sample(point)) << "failure at exchange " << failure;
    EXPECT_EQ(123, point.x);
    EXPECT_EQ((std::vector<uint8_t>{0x82, 0, 0}), last_attempt);
    EXPECT_FALSE(hardware.touch.isSelected());
    hardware.touch.clear();
    fail_transfer_at = -1;
    EXPECT_EQ(1, touch_->sample(point));
    expectFrameCleanup();
  }
}

// Verifies a failed native registration before bus initialization disables
// samples and permits retry once the application has initialized the bus.
TEST_F(TouchXpt2046Test, NativeInitializationRetry) {
  touch_.reset();
  spi_.deinit();
  RawTouch<> fresh(spi_);
  fresh.initTouch();
  TouchPoint point;
  EXPECT_EQ(0, fresh.sample(point));
  EXPECT_TRUE(hardware.touch.frames.empty());
  spi_.init(18, 19, 23);
  fresh.initTouch();
  EXPECT_EQ(1, fresh.sample(point));
  expectFrameCleanup();
}

#endif
// Verifies a sensor whose pressure keeps increasing cannot hold the SPI
// reservation forever while waiting for a settled conversion.
TEST(Xpt2046Protocol, BoundsPressureSettling) {
  struct IncreasingPressure {
    bool transfer(const uint8_t*, uint8_t* rx, size_t len) {
      EXPECT_EQ(5u, len);
      const uint16_t sample = (100 + calls++) << 3;
      rx[0] = 0;
      rx[1] = sample >> 8;
      rx[2] = sample;
      rx[3] = 0x7f;
      rx[4] = 0xf8;
      return true;
    }
    int calls = 0;
  } spi;
  uint16_t x = 0;
  uint16_t y = 0;
  uint16_t z = 0;
  EXPECT_EQ(roo_display::internal::XptConversion::kUnsettled,
            roo_display::internal::ReadXptConversion(spi, 400, x, y, z));
  EXPECT_LE(spi.calls, kMaxConversionAttempts);
}

}  // namespace

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "roo_display/driver/st7701.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/voltage/voltage.h"

namespace {

constexpr int8_t kCsPin = 17;
constexpr int8_t kClockPin = 18;
constexpr int8_t kDataPin = 19;

struct Spi9BitFixture {
  Spi9BitFixture()
      : cs("st7701_cs"),
        data("st7701_data"),
        clock(roo_testing_transducers::SimpleDigitalSink::WithSignalCallback(
            "st7701_clock",
            [&](const roo_testing_transducers::VoltageSignal& signal) {
              using namespace roo_testing_transducers;
              if (DigitalLevelFromVoltage(AverageDcVoltage(signal)) ==
                  kDigitalHigh) {
                bits.push_back(data.isHigh() ? 1 : 0);
              }
            })) {
    FakeEsp32().gpio.attachOutput(kCsPin, cs);
    FakeEsp32().gpio.attachOutput(kClockPin, clock);
    FakeEsp32().gpio.attachOutput(kDataPin, data);
  }

  ~Spi9BitFixture() {
    FakeEsp32().gpio.detach(kCsPin);
    FakeEsp32().gpio.detach(kClockPin);
    FakeEsp32().gpio.detach(kDataPin);
  }

  std::vector<int> bits;
  roo_testing_transducers::SimpleDigitalSink cs;
  roo_testing_transducers::SimpleDigitalSink data;
  roo_testing_transducers::SimpleDigitalSink clock;
};

// Verifies the 9-bit transport sends a command flag followed by its byte, then
// a data flag followed by its byte, while restoring the idle GPIO levels.
TEST(St7701Driver, Spi9BitCommandFraming) {
  Spi9BitFixture fixture;
  roo_display::st7701::Spi9Bit target(kCsPin, kClockPin, kDataPin);
  target.init();
  EXPECT_TRUE(fixture.cs.isHigh());
  EXPECT_TRUE(fixture.clock.isLow());

  target.writeCommand(roo_display::st7701::BK0_LNESET, {0xA5});

  EXPECT_EQ(fixture.bits, std::vector<int>({0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0,
                                            1, 0, 0, 1, 0, 1}));
  EXPECT_TRUE(fixture.cs.isHigh());
  EXPECT_TRUE(fixture.clock.isLow());
}

}  // namespace

#include <Arduino.h>

#include <array>

#include "gtest/gtest.h"
#include "roo_display/hal/arduino/spi.h"
#include "roo_display/hal/gpio.h"
#include "roo_display/hal/spi_settings.h"
#include "roo_testing/devices/display/st77xx/st77xx.h"
#include "roo_testing/microcontrollers/rp2040/fake_pico.h"

namespace {
/// Captures a small panel without a GUI server.
class Framebuffer : public roo_testing_transducers::Viewport {
 public:
  /// Fills inclusive rectangles from the existing ST77xx device model.
  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                uint32_t color) override {
    ASSERT_GE(x0, 0);
    ASSERT_GE(y0, 0);
    ASSERT_LT(x1, 4);
    ASSERT_LT(y1, 4);
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) pixels[y * 4 + x] = color;
    }
  }

  /// Copies a model-supplied rectangle.
  void drawRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                const uint32_t* colors) override {
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) fillRect(x, y, x, y, *colors++);
    }
  }
  std::array<uint32_t, 16> pixels{};
};

using Settings = roo_display::SpiSettings<8000000, roo_display::kSpiMsbFirst,
                                          roo_display::kSpiMode0>;

// Verifies the existing Arduino transport writes a frame through the shared
// ST77xx model, preserving const pixel input and byte order across fill/bulk
// APIs.
TEST(PicoSpiTransport, SharedDisplayModel) {
  Framebuffer viewport;
  FakeSt77xxSpi panel(viewport, 4, 4);
  FakePico().attachSpiDevice(panel, 18, -1, 19);
  FakePico().attachOutput(17, panel.cs());
  FakePico().attachOutput(20, panel.dc());
  FakePico().attachOutput(21, panel.rst());
  for (int pin : {17, 20, 21}) {
    roo_display::DefaultGpio::setOutput(pin);
    roo_display::DefaultGpio::setHigh(pin);
  }
  roo_display::ArduinoSpi spi;
  spi.init(18, 16, 19);
  roo_display::ArduinoSpiDevice<Settings> device(spi);
  device.beginWriteOnlyTransaction();
  auto command = [&device](uint8_t cmd) {
    digitalWrite(17, LOW);
    digitalWrite(20, LOW);
    device.write(cmd);
    digitalWrite(20, HIGH);
  };
  command(1);
  digitalWrite(17, HIGH);
  command(0x3a);
  device.write(0x55);
  digitalWrite(17, HIGH);
  command(0x2a);
  device.write16x2(0, 3);
  digitalWrite(17, HIGH);
  command(0x2b);
  device.write16x2(0, 3);
  digitalWrite(17, HIGH);
  command(0x2c);
  const roo::byte green[] = {roo::byte{0x07}, roo::byte{0xe0}};
  device.write16(0xf800);
  device.writeBytes(green, 2);
  device.fill16(green, 14);
  digitalWrite(17, HIGH);
  device.endTransaction();
  EXPECT_EQ(viewport.pixels[0], 0xffff0000u);
  for (size_t i = 1; i < viewport.pixels.size(); ++i)
    EXPECT_EQ(viewport.pixels[i], 0xff00ff00u);
  EXPECT_EQ(green[0], roo::byte{0x07});
  EXPECT_EQ(green[1], roo::byte{0xe0});
  SPI.end();
}
}  // namespace

// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#basic-setup

#include "Arduino.h"
#include "roo_display.h"

using namespace roo_display;

// Select the driver to match your display device.
#include "roo_display/driver/ili9341.h"

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 2;
static constexpr int kRstPin = 4;
static constexpr int kBlPin = 16;

static constexpr int kSpiSck = -1;
static constexpr int kSpiMiso = -1;
static constexpr int kSpiMosi = -1;

// Uncomment if you have connected the BL pin to GPIO.

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

void setup() {
  // Use default SPI pins, or specify your own.
  // SPI.begin();
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);

  // Set the background color, and clear the screen.
  display.init(color::White);

  // Uncomment if using backlit.
  // backlit.begin();
}

void loop() {
  // Uncomment to control backlit.
  // backlit.setIntensity(1 + (millis() / 5) % 256);
}

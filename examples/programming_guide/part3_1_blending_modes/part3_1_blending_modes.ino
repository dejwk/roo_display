// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#blending-modes

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

#include "roo_display/color/gradient.h"
#include "roo_display/shape/smooth.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(color::LightSeaGreen);

  // Uncomment if using backlit.
  // backlit.begin();
}

void loop() {
  auto circle = SmoothThickCircle({100, 100}, 71, 42, Color(0xFFEC6D44));
  auto rect1 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, M_PI / 4, color::White);
  auto rect2 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, -M_PI / 4, color::White);
  auto gradient = RadialGradientSq(
      {100, 100}, ColorGradient({{50 * 50, color::Black.withA(0x60)},
                                 {65 * 65, color::Black.withA(0x15)},
                                 {71 * 71, color::Transparent},
                                 {77 * 77, color::Black.withA(0x15)},
                                 {92 * 92, color::Black.withA(0x60)}}));
  auto shadow =
      SmoothThickCircle({100 - 5, 100 + 5}, 71, 42, color::Black.withA(0x80));

  Offscreen<Argb4444> offscreen(201, 201);
  {
    DrawingContext dc(offscreen);
    dc.clear();
    dc.draw(circle);
    dc.setBlendingMode(BLENDING_MODE_SOURCE_ATOP);
    dc.draw(rect1);
    dc.draw(rect2);
    dc.draw(gradient);
    dc.setBlendingMode(BLENDING_MODE_DESTINATION_OVER);
    dc.draw(shadow);
  }

  DrawingContext dc(display);
  dc.draw(offscreen, kCenter | kMiddle);

  delay(10000);
}

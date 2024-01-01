// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#backgrounds-and-overwriting

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

// Uncomment if you have connected the BL pin to GPIO.

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin, /* ledc channel */ 0);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Regular/27.h"

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void loop() {
  DrawingContext dc(display);
  // Comment out this line to see the difference.
  dc.setFillMode(FILL_MODE_RECTANGLE);
  dc.draw(
    TextLabel("12:00 ", font_NotoSans_Regular_27(), color::Blue),
    5, 30);
  dc.draw(
    TextLabel("21:41 ", font_NotoSans_Regular_27(), color::Red),
    5, 30);

  delay(10000);
}

// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#using-the-background-fill-optimizer

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

#include "roo_display/filter/background_fill_optimizer.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_Italic/90.h"

BackgroundFillOptimizerDevice fill_optimizer(device);
Display display(fill_optimizer);

void setup() {
  SPI.begin();
  // For simplicity, only using 1-color palette. (You can use up to 15 colors).
  fill_optimizer.setPalette({color::White});
  display.init(color::White);
}

void loop() {
  DrawingContext dc(display);
  dc.clear();
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          rand() % display.width(), rand() % display.height());
  delay(5);
}

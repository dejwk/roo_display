// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#drawing-individual-pixels

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

#include "roo_display/filter/transformation.h"

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void basic() {
  display.clear();
  DrawingContext dc(display);
  dc.drawPixels([](PixelWriter& writer) {
    for (int i = 0; i < 319; ++i) {
      writer.writePixel(i, 100 + (int)(20 * sin(i / 20.0)), color::Black);
    }
    for (int i = 0; i < 319; ++i) {
      writer.writePixel(i, 180 + (int)(20 * cos(i / 20.0)), color::Red);
    }
  });
  delay(2000);
}

void scaled() {
  display.clear();
  DrawingContext dc(display);
  dc.setClipBox(20, 20, 299, 219);
  dc.setTransformation(Transformation().scale(3, 3));
  dc.drawPixels([](PixelWriter& writer) {
    for (int i = 0; i < 100; ++i) {
      writer.writePixel(i, 30 + (int)(10 * sin(i / 10.0)), color::Black);
    }
    for (int i = 0; i < 100; ++i) {
      writer.writePixel(i, 50 + (int)(10 * cos(i / 10.0)), color::Red);
    }
  });
  delay(2000);
}

void loop() {
  basic();
  scaled();
}

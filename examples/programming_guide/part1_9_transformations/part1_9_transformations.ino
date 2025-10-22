// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#transformations

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

#include "roo_display/filter/transformation.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/27.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(Graylevel(0xF0));

  // Uncomment if using backlit.
  // backlit.begin();
}

void loop() {
  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(10, 3));
  dc.draw(TextLabel("F", font_NotoSerif_Italic_27(), color::Black),
          kCenter | kMiddle);
  dc.setTransformation(Transformation().rotateLeft());
  dc.draw(TextLabel("Hello, World!", font_NotoSerif_Italic_27(), color::Black),
          kLeft | kMiddle);
  dc.setTransformation(Transformation().rotateRight().flipX());
  dc.draw(TextLabel("Hello, World!", font_NotoSerif_Italic_27(), color::Black),
          kRight | kMiddle);
  delay(10000);
}

// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#touch

#include "Arduino.h"
#include "roo_display.h"

using namespace roo_display;

// Select the driver to match your display device.
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 2;
static constexpr int kRstPin = 4;
static constexpr int kBlPin = 16;
static constexpr int kTouchCsPin = 27;

static constexpr int kSpiSck = -1;
static constexpr int kSpiMiso = -1;
static constexpr int kSpiMosi = -1;

// Uncomment if you have connected the BL pin to GPIO.

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin);

Ili9341spi<kCsPin, kDcPin, kRstPin> display_device(Orientation().rotateLeft());
TouchXpt2046<kTouchCsPin> touch_device;

// See
// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#calibration

Display display(display_device, touch_device,
                TouchCalibration(269, 249, 3829, 3684,
                                 Orientation::LeftDown()));

#include "roo_display/shape/basic.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(color::DarkGray);

  // Uncomment if using backlit.
  // backlit.begin();
}

int16_t x = -1;
int16_t y = -1;
bool was_touched = false;

void loop(void) {
  int16_t old_x = x;
  int16_t old_y = y;
  bool touched = display.getTouch(x, y);
  if (touched) {
    was_touched = true;
    DrawingContext dc(display);
    dc.draw(Line(0, y, display.width() - 1, y, color::Red));
    dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
    if (x != old_x) {
      dc.draw(
          Line(old_x, 0, old_x, display.height() - 1, dc.getBackgroundColor()));
    }
    if (y != old_y) {
      dc.draw(
          Line(0, old_y, display.width() - 1, old_y, dc.getBackgroundColor()));
    }
    dc.draw(Line(0, y, display.width() - 1, y, color::Red));
    dc.draw(Line(x, 0, x, display.height() - 1, color::Red));
  } else {
    if (was_touched) {
      was_touched = false;
      DrawingContext dc(display);
      dc.draw(
          Line(0, old_y, display.width() - 1, old_y, dc.getBackgroundColor()));
      dc.draw(
          Line(old_x, 0, old_x, display.height() - 1, dc.getBackgroundColor()));
    }
  }
}

// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#tiles

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

#include "roo_display/shape/basic.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSans_Regular/27.h"

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void basic() {
  display.clear();
  const auto& font = font_NotoSans_Regular_27();
  TextLabel label1("Hello, hello, world!", font, color::Black);
  TextLabel label2("Hello, world!", font, color::Black);
  Box extents(10, 10, 309, 49);
  Alignment a = kLeft.shiftBy(5) | kMiddle;
  DrawingContext dc(display);
  Tile tile1(&label1, extents, a, color::Gainsboro);
  dc.draw(tile1);
  delay(2000);
  Tile tile2(&label2, extents, a, color::Gainsboro);
  dc.draw(tile2);
  delay(2000);
}

void using_MakeTileOf() {
  display.clear();
  const auto& font = font_NotoSans_Regular_27();
  Box extents(10, 10, 309, 49);
  Alignment a = kLeft.shiftBy(5) | kMiddle;
  DrawingContext dc(display);
  dc.draw(MakeTileOf(TextLabel("Hello, hello, world!", font, color::Black),
                     extents, a, color::Gainsboro));
  delay(2000);
  dc.draw(MakeTileOf(TextLabel("Hello, world!", font, color::Black), extents, a,
                     color::Gainsboro));
  delay(2000);
}

void with_background() {
  {
    int w = display.width();
    int h = display.height();
    DrawingContext dc(display);
    dc.draw(FilledRect(0, h / 2, w - 1, h - 1, color::Khaki));
  }
  auto tile = MakeTileOf(
      TextLabel("Hello, world!", font_NotoSans_Regular_27(), color::Black),
      Box(10, 10, 309, 49), kLeft.shiftBy(5) | kMiddle, color::Red.withA(0x30));
  DrawingContext dc(display);
  dc.draw(tile);
  dc.setBackgroundColor(color::Khaki);
  dc.draw(tile, 0, display.height() / 2);
  delay(2000);
}

void loop() {
  basic();
  using_MakeTileOf();
  with_background();
}

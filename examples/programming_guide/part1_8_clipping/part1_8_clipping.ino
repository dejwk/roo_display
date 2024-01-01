// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#clipping

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
#include "roo_fonts/NotoSerif_Italic/60.h"

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void with_clip_box() {
  display.clear();

  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  int w = display.width();
  int h = display.height();
  DrawingContext dc(display);
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  dc.setClipBox(0, 0, w - 1, h / 2 - 1);
  label.setColor(color::Maroon);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipBox(0, h / 2, w - 1, h - 1);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);

  delay(2000);
}

// 320x60 pixels.
uint8_t mask_data[320 * 60 / 8];

void with_clip_mask() {
  display.clear();

  for (int i = 0; i < 30; i++) {
    // Alternate rows: bits set, bits cleared.
    memset(mask_data + 2 * i * 320 / 8, 0xFF, 320 / 8);
    memset(mask_data + (2 * i + 1) * 320 / 8, 0x00, 320 / 8);
  }

  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  int w = display.width();
  int h = display.height();
  DrawingContext dc(display);
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  ClipMask mask1(mask_data, Box(0, 90, 319, 149));
  dc.setClipMask(&mask1);
  label.setColor(color::DarkSalmon);
  dc.draw(label, kCenter | kMiddle);
  // Same as before, but shifted by one row.
  ClipMask mask2(mask_data, Box(0, 91, 319, 150));
  dc.setClipMask(&mask2);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipMask(nullptr);  // To be on the safe side.

  delay(2000);
}

void loop() {
  with_clip_box();
  with_clip_mask();
}
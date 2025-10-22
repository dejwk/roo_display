// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#using-off-screen-buffers

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

#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/27.h"
#include "roo_fonts/NotoSerif_Italic/60.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(Graylevel(0xF0));

  // Uncomment if using backlit.
  // backlit.begin();
}

void antialiasing() {
  display.clear();

  Offscreen<Rgb565> offscreen(200, 140, color::White);
  {
    DrawingContext dc(offscreen);
    dc.draw(SmoothFilledCircle({150, 50}, 40, color::Navy));
    dc.draw(SmoothFilledCircle({80, 80}, 70, color::Red.withA(0x90)));
    dc.draw(TextLabel("Oh, my!", font_NotoSerif_Italic_27(), color::Yellow), 70,
            86);
  }
  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(2, 2));
  dc.draw(offscreen, kCenter | kMiddle);

  delay(2000);
}

static constexpr Color c = color::Cyan.withA(0x99);
static constexpr Color m = color::Magenta.withA(0x99);
static constexpr Color y = color::Yellow.withA(0x99);

void indexed_color_modes_static_palette() {
  static Color colors[] = {color::Transparent,
                           c,
                           m,
                           y,
                           AlphaBlend(c, m),
                           AlphaBlend(m, y),
                           AlphaBlend(c, y),
                           AlphaBlend(AlphaBlend(c, m), y)};

  // Create a palette suitable for drawing to an offscreen, consisting of the
  // specifed colors.
  static Palette palette = Palette::ReadWrite(colors, 8);

  display.clear();

  Offscreen<Indexed4> offscreen(150, 150, color::Transparent,
                                Indexed4(&palette));
  {
    DrawingContext dc(offscreen);
    dc.draw(FilledCircle::ByRadius(50, 50, 50, c));
    dc.draw(FilledCircle::ByRadius(90, 50, 50, m));
    dc.draw(FilledCircle::ByRadius(70, 80, 50, y));
  }
  DrawingContext dc(display);
  dc.draw(offscreen, kCenter | kMiddle);

  delay(2000);
}

void indexed_color_modes_dynamic_palette() {
  display.clear();

  // Declare a dynamic palette with up-to 16 colors.
  Palette palette = Palette::Dynamic(16);
  Offscreen<Indexed4> offscreen(150, 150, color::Transparent,
                                Indexed4(&palette));
  {
    DrawingContext dc(offscreen);
    dc.draw(FilledCircle::ByRadius(50, 50, 50, c));
    dc.draw(FilledCircle::ByRadius(90, 50, 50, m));
    dc.draw(FilledCircle::ByRadius(70, 80, 50, y));
  }
  DrawingContext dc(display);
  dc.draw(offscreen, kCenter | kMiddle);

  delay(2000);
}

void clip_masks() {
  static uint8_t mask_data[320 * 60 / 8];

  BitMaskOffscreen offscreen(320, 60, mask_data);
  {
    DrawingContext dc(offscreen);
    dc.clear();
    for (int i = 0; i < 320; ++i) {
      int y = (int)(10 * sin(i / 10.0)) + 30;
      // Anything but color::Transparent is interpreted as 'bit set'.
      dc.draw(Line(i, 0, i, y, color::Black));
    }
  }

  const auto& font = font_NotoSerif_Italic_60();
  auto label = TextLabel("Hello!", font, color::Black);
  DrawingContext dc(display);
  int w = dc.width();
  int h = dc.height();
  dc.setClipBox(0, 0, w - 1, 179);
  dc.draw(FilledCircle::ByRadius(w / 2, 179, w / 2 - 20, color::Gold));
  dc.setBackgroundColor(color::Gold);
  ClipMask mask(mask_data, Box(0, 90, 319, 149));
  dc.setClipMask(&mask);
  label.setColor(color::Crimson);
  dc.draw(label, kCenter | kMiddle);
  mask.setInverted(true);
  label.setColor(color::MidnightBlue);
  dc.draw(label, kCenter | kMiddle);
  dc.setClipMask(nullptr);  // To be on the safe side.

  delay(2000);
}

void loop() {
  antialiasing();
  indexed_color_modes_static_palette();
  indexed_color_modes_dynamic_palette();
  clip_masks();
}

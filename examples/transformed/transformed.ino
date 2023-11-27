#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/st77xx/st77xx.h"
#include "roo_testing/devices/microcontroller/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeSt77xxSpi display;

  Emulator()
      : viewport(), flexViewport(viewport, 2), display(flexViewport, 240, 240) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(2, display.dc());
    FakeEsp32().gpio.attachOutput(4, display.rst());
  }
} emulator;

#endif

#include <string>

#include "roo_display.h"
#include "roo_display/filter/transformation.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/27.h"

using namespace roo_display;

// This example showcases transformation capabilities.
//
// Every drawable object can be rescaled (by integer factors), moved, flipped,
// and rotated (by multiple of 90 degrees). This is most useful for things like
// string labels, as is demonstrated in this example.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h"
St7789spi_240x240<5, 2, 4> device;

Display display(device);

void setup() {
  srand(time(nullptr));
  Serial.begin(9600);
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init(color::White);
  if (display.height() > display.width()) {
    display.setOrientation(Orientation().rotateRight());
  }
}

void loop(void) {
  int16_t x_scale, y_scale;
  do {
    x_scale = rand() % 7 - 3;
  } while (x_scale == 0);
  do {
    y_scale = rand() % 7 - 3;
  } while (y_scale == 0);
  int16_t rotations = rand() % 4;
  Color fg = AlphaBlend(Color(rand(), rand(), rand()), Color(0xE0000000));
  Color bg(rand(), rand(), rand());
  int16_t x = rand() % (3 * display.width()) - display.width();
  int16_t y = rand() % (3 * display.height()) - display.height();
  uint16_t c;
  GlyphMetrics gm;
  do {
    c = rand();
  } while (!font_NotoSerif_BoldItalic_27().getGlyphMetrics(
      c, FONT_LAYOUT_HORIZONTAL, &gm));
  uint8_t utf8[4];
  if (c < 0x80) {
    utf8[0] = c;
    utf8[1] = 0;
  } else if (c < 0x0800) {
    utf8[0] = ((c >> 6) & 0x1F) | 0xC0;
    utf8[1] = (c & 0x3F) | 0x80;
    utf8[2] = 0;
  } else {
    utf8[0] = ((c >> 12) & 0x0F) | 0xE0;
    utf8[1] = ((c >> 6) & 0x3F) | 0x80;
    utf8[2] = (c & 0x3F) | 0x80;
    utf8[3] = 0;
  }
  StringViewLabel label((const char*)utf8, font_NotoSerif_BoldItalic_27(), fg,
                        FILL_MODE_RECTANGLE);
  DrawingContext dc(display);
  dc.setBackgroundColor(bg);
  dc.setTransformation(Transformation()
                           .rotateClockwise(rotations)
                           .scale(x_scale, y_scale)
                           .translate(x, y));
  dc.draw(label);
}

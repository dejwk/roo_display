#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/st77xx/st77xx.h"
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
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/15.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/27.h"
#include "roo_smooth_fonts/NotoSerif_BoldItalic/40.h"

using namespace roo_display;

// This example shows alignment options.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h"
St7789spi_240x240<5, 2, 4> device;

Display display(device);

void setup() {
  Serial.begin(9600);
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init(color::White);
  if (display.height() > display.width()) {
    display.setOrientation(Orientation().rotateRight());
  }
}

void simpleAlignment() {
  // Draw text aligned to the corners, sides, and the middle of the screen.
  // Note that some glyphs may be clipped (e.g. the left of the 'j' letter).
  // This is by design - alignment rules for text allow it to sometimes
  // have parts 'stick out' so that the alignment looks good to the eye.
  // This is why you generally should use padding when working with aligned
  // text. (See simpleAlignmentWithPadding, below).
  DrawingContext dc(display);
  StringViewLabel label("jW", font_NotoSerif_BoldItalic_27(), color::Black);

  dc.clear();

  dc.draw(label, kTop | kLeft);
  dc.draw(label, kTop | kCenter);
  dc.draw(label, kTop | kRight);

  dc.draw(label, kMiddle | kLeft);
  dc.draw(label, kMiddle | kCenter);
  dc.draw(label, kMiddle | kRight);

  dc.draw(label, kBottom | kLeft);
  dc.draw(label, kBottom | kCenter);
  dc.draw(label, kBottom | kRight);
}

void simpleAlignmentWithPadding() {
  // This example show how to add some absolute padding to the alignment.
  // In this case, we're moving the text 10 pixels into the center.
  DrawingContext dc(display);
  StringViewLabel label("jW", font_NotoSerif_BoldItalic_27(), color::Black);

  dc.clear();

  auto top = kTop.shiftBy(10);
  auto bottom = kBottom.shiftBy(-10);

  auto left = kLeft.shiftBy(10);
  auto right = kRight.shiftBy(-10);

  dc.draw(label, top | left);
  dc.draw(label, top | kCenter);
  dc.draw(label, top | right);

  dc.draw(label, kMiddle | left);
  dc.draw(label, kMiddle | kCenter);
  dc.draw(label, kMiddle | right);

  dc.draw(label, bottom | left);
  dc.draw(label, bottom | kCenter);
  dc.draw(label, bottom | right);
}

void baselineAlignment() {
  // This example shows how to align text baselines.
  // By convention, roo_display assumes that the baseline is at zero. In
  // particular, TextLabel and StringViewLabel always have bounding boxes so
  // that the origin (0, 0) is at the baseline of the text. In our example
  // below, we align these baselines to the middle of the screen. (It is also
  // possible to align them to top or bottom - just change 'toMiddle' to
  // something else, and add some absolute offset with offsetBy()).

  DrawingContext dc(display);
  StringViewLabel label1("Hop ", font_NotoSerif_BoldItalic_15(), color::Black);
  StringViewLabel label2("Hop ", font_NotoSerif_BoldItalic_40(), color::Black);
  StringViewLabel label3("Hooray", font_NotoSerif_BoldItalic_27(),
                         color::Black);

  dc.clear();
  dc.draw(Line(0, 0, display.width() - 1, 0, color::Red), kMiddle);

  auto left = kLeft.shiftBy(5);
  dc.draw(label1, left | kBaseline.toMiddle());
  left = left.shiftBy(label1.metrics().advance());
  dc.draw(label2, left | kBaseline.toMiddle());
  left = left.shiftBy(label2.metrics().advance());
  dc.draw(label3, left | kBaseline.toMiddle());
}

void subregionAlignment() {
  // You can create a drawing context constrained to specific bounds, so that
  // alignment applies to these bounds, and not the entire device.

  DrawingContext left(display,
                      Box(0, 0, display.width() / 2 - 1, display.height() - 1));
  DrawingContext right(display, Box(display.width() / 2, 0, display.width() - 1,
                                    display.height() - 1));
  left.clear();
  right.clear();
  left.draw(StringViewLabel("L", font_NotoSerif_BoldItalic_40(), color::Black),
            kMiddle | kCenter);
  right.draw(StringViewLabel("R", font_NotoSerif_BoldItalic_40(), color::Black),
             kMiddle | kCenter);
}

void tile() {
  // Note: when drawing a tile, you don't need to worry about clearing the
  // background first.
  DrawingContext dc(display);
  dc.draw(MakeTileOf(
      StringViewLabel("Top", font_NotoSerif_BoldItalic_40(), color::Black),
      Box(0, 0, display.width() - 1, display.height() / 2 - 1),
      kBottom.shiftBy(-10) | kCenter, color::Bisque));
  dc.draw(MakeTileOf(
      StringViewLabel("Bottom", font_NotoSerif_BoldItalic_40(), color::Black),
      Box(0, display.height() / 2, display.width() - 1, display.height() - 1),
      kTop.shiftBy(10) | kCenter, color::Beige));
}

void loop(void) {
  simpleAlignment();
  delay(2000);
  simpleAlignmentWithPadding();
  delay(2000);
  baselineAlignment();
  delay(2000);
  subregionAlignment();
  delay(2000);
  tile();
  delay(2000);
}

#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9341Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(),
        flexViewport(viewport, 6, FlexViewport::kRotationRight),
        display(flexViewport),
        touch(flexViewport, FakeXpt2046Spi::Calibration(318, 346, 3824, 3909,
                                                        false, true, false)) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());

    FakeEsp32().attachSpiDevice(touch, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(2, touch.cs());
  }
} emulator;

#endif

#include "SPI.h"
#include "roo_display.h"
#include "roo_display/backlit/esp32_ledc.h"
#include "roo_display/color/gradient.h"
#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_display/filter/foreground.h"
#include "roo_display/font/font.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/string_printer.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Bold/27.h"
#include "roo_fonts/NotoSans_Condensed/12.h"

using namespace roo_display;

Ili9341spi<5, 17, 27> display_device(Orientation().rotateLeft());

Display display(display_device);

LedcBacklit backlit(16, 1);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::White);
}

void loop() {
  FpPoint center{display.width() / 2.0 - 0.5, display.height() / 2.0 - 0.5};
  for (int i = 0; i <= 40; i += 1) {
    DrawingContext dc(display);
    dc.draw(SmoothThickArc(center, 71, 42, -2.0, 2.0,
                           color::Navy.withA(i * 6 + 15)));
  }
  auto arc = SmoothThickArc(center, 71, 42, -2.0, 2.0, color::Black);
  for (int i = 0; i <= 20; i += 2) {
    Color c = AlphaBlend(color::Navy, color::Gold.withA(i * 12 + 15));
    auto gradient =
        AngularGradient(center, ColorGradient({{-2.1, color::Navy}, {2.1, c}}));

    RasterizableStack stack(arc.extents());
    stack.addInput(&arc);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack);
  }
  for (int i = 0; i <= 20; i += 1) {
    auto gradient =
        AngularGradient(center, ColorGradient({{-2.1 - i * 0.5, color::Navy},
                                               {2.0 - i * 0.4, color::Gold},
                                               {4.1 - i * 0.25, color::Red},
                                               {10, color::Red}}));

    RasterizableStack stack(arc.extents());
    stack.addInput(&arc);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack);
  }
  for (int i = 0; i <= 20; i += 1) {
    auto gradient = LinearGradient({(int)center.x, (int)center.y}, 1, 0.2,
                                   ColorGradient({{-150, color::Red},
                                                  {(i - 12) * 15, color::Red},
                                                  {(i - 10) * 15, color::Gold},
                                                  {(i - 8) * 15, color::Red},
                                                  {150, color::Red}}));

    RasterizableStack stack(arc.extents());
    stack.addInput(&arc);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack);
  }

  for (int i = 0; i <= 40; i += 1) {
    DrawingContext dc(display);
    dc.draw(SmoothThickArc(center, 71, 42, -2.0, 2.0,
                           color::Red.withA(255 - (i * 6 + 15))));
  }
  {
    DrawingContext dc(display);
    dc.clear();
  }

  delay(1000);
}
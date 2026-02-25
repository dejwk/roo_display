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

  Emulator()
      : viewport(),
        flexViewport(viewport, 6, FlexViewport::kRotationRight),
        display(flexViewport) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());
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

using namespace roo_display;

Ili9341spi<5, 17, 27> display_device(Orientation().rotateLeft());

Display display(display_device);

// LedcBacklit backlit(16, 1);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::White);
}

void loop() {
  FpPoint center{display.width() / 2.0f - 0.5f, display.height() / 2.0f - 0.5f};
  for (int i = 0; i <= 40; i += 1) {
    DrawingContext dc(display);
    dc.draw(SmoothThickArc(center, 71, 42, -2.0f, 2.0f,
                           color::Navy.withA(i * 6 + 15)));
  }
  auto arc = SmoothThickArc(center, 71, 42, -2.0f, 2.0f, color::Black);
  for (int i = 0; i <= 20; i += 2) {
    Color c = AlphaBlend(color::Navy, color::Gold.withA(i * 12 + 15));
    auto gradient = AngularGradient(
        center, ColorGradient({{-2.1f, color::Navy}, {2.1f, c}}));

    RasterizableStack stack(arc.extents());
    stack.addInput(&arc);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack);
  }
  for (int i = 0; i <= 20; i += 1) {
    auto gradient =
        AngularGradient(center, ColorGradient({{-2.1f - i * 0.5f, color::Navy},
                                               {2.0f - i * 0.4f, color::Gold},
                                               {4.1f - i * 0.25f, color::Red},
                                               {10.0f, color::Red}}));

    RasterizableStack stack(arc.extents());
    stack.addInput(&arc);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack);
  }
  for (int i = 0; i <= 20; i += 1) {
    auto gradient =
        LinearGradient({(int16_t)center.x, (int16_t)center.y}, 1, 0.2,
                       ColorGradient({{-150.0f, color::Red},
                                      {(i - 12) * 15.0f, color::Red},
                                      {(i - 10) * 15.0f, color::Gold},
                                      {(i - 8) * 15.0f, color::Red},
                                      {150.0f, color::Red}}));

    RasterizableStack stack(arc.extents());
    stack.addInput(&arc);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack);
  }

  for (int i = 0; i <= 40; i += 1) {
    DrawingContext dc(display);
    dc.draw(SmoothThickArc(center, 71, 42, -2.0f, 2.0f,
                           color::Red.withA(255 - (i * 6 + 15))));
  }
  {
    DrawingContext dc(display);
    dc.clear();
  }

  delay(1000);
}
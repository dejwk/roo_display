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
TouchXpt2046<2> touch_device;

Display display(display_device, touch_device);

LedcBacklit backlit(16);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::LightSeaGreen);
  backlit.begin();
}

void loop() {
  auto circle = SmoothThickCircle({100, 100}, 71, 42, Color(0xFFEC6D44));
  auto rect1 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, M_PI / 4, color::White);
  auto rect2 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, -M_PI / 4, color::White);
  auto gradient = RadialGradientSq(
      {100, 100}, ColorGradient({{50 * 50, color::Black.withA(0x60)},
                                 {65 * 65, color::Black.withA(0x15)},
                                 {71 * 71, color::Transparent},
                                 {77 * 77, color::Black.withA(0x15)},
                                 {92 * 92, color::Black.withA(0x60)}}));
  auto shadow =
      SmoothThickCircle({100 - 5, 100 + 5}, 71, 42, color::Black.withA(0x80));

  {
    DrawingContext dc(display);
    dc.clear();
  }
  for (int i = 0; i <= 20; i += 1) {
    DrawingContext dc(display);
    dc.draw(SmoothThickCircle({100, 100}, 71, 42,
                              Color(0xFFeC6D44).withA(i * 12 + 15)),
            kCenter | kMiddle);
  }
  for (int i = 0; i <= 20; i += 2) {
    RasterizableStack stack(Box(0, 0, 200, 200));
    stack.addInput(&circle);
    stack.addInput(&rect1, -4 * (i - 20), 4 * (i - 20))
        .withMode(kBlendingSourceAtop);
    stack.addInput(&rect2, 4 * (i - 20), 4 * (i - 20))
        .withMode(kBlendingSourceAtop);
    DrawingContext dc(display);
    dc.draw(stack, kCenter | kMiddle);
  }
  {
    for (int i = 0; i <= 5; ++i) {
      RasterizableStack stack(Box(0, 0, 200, 200));
      stack.addInput(&circle);
      stack.addInput(&rect1).withMode(kBlendingSourceAtop);
      stack.addInput(&rect2).withMode(kBlendingSourceAtop);
      stack.addInput(&shadow, 5 - i, i - 5).withMode(kBlendingDestinationOver);
      DrawingContext dc(display);
      dc.draw(stack, kCenter | kMiddle);
    }
  }
  {
    RasterizableStack stack(Box(0, 0, 200, 200));
    stack.addInput(&circle);
    stack.addInput(&rect1).withMode(kBlendingSourceAtop);
    stack.addInput(&rect2).withMode(kBlendingSourceAtop);
    stack.addInput(&gradient).withMode(kBlendingSourceAtop);
    stack.addInput(&shadow).withMode(kBlendingDestinationOver);
    {
      DrawingContext dc(display);
      dc.draw(stack, kCenter | kMiddle);
    }
  }
  delay(1000);
  {
    DrawingContext dc(display);
    for (int i = 0; i <= 10; i += 2) {
      auto fill = Fill(color::Black.withA((10 - i) * 20));
      RasterizableStack stack(Box(0, 0, 200, 200));
      stack.addInput(&circle);
      stack.addInput(&rect1).withMode(kBlendingSourceAtop);
      stack.addInput(&rect2).withMode(kBlendingSourceAtop);
      stack.addInput(&gradient).withMode(kBlendingSourceAtop);
      stack.addInput(&shadow).withMode(kBlendingDestinationOver);
      stack.addInput(&fill).withMode(kBlendingDestinationIn);
      dc.draw(stack, kCenter | kMiddle);
    }
    dc.clear();
  }
  delay(1000);
}

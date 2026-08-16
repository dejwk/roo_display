#include "Arduino.h"

// Both displays share these SPI bus pins.
static constexpr int kSpiSck = 18;
static constexpr int kSpiMiso = 19;
static constexpr int kSpiMosi = 23;

// Each display has its own chip-select, data/command, and reset pins.
static constexpr int kIli9341Cs = 5;
static constexpr int kIli9341Dc = 2;
static constexpr int kIli9341Rst = 4;
static constexpr int kSt7789Cs = 15;
static constexpr int kSt7789Dc = 17;
static constexpr int kSt7789Rst = 16;

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/display/st77xx/st77xx.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport ili9341_viewport;
  FlexViewport ili9341_flex_viewport;
  FakeIli9341Spi ili9341;

  FltkViewport st7789_viewport;
  FlexViewport st7789_flex_viewport;
  FakeSt77xxSpi st7789;

  Emulator()
      : ili9341_flex_viewport(ili9341_viewport, 1),
        ili9341(ili9341_flex_viewport),
        st7789_flex_viewport(st7789_viewport, 1),
        st7789(st7789_flex_viewport, 240, 240) {
    // Both emulated devices are attached to the same SPI bus.
    FakeEsp32().attachSpiDevice(ili9341, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().attachSpiDevice(st7789, kSpiSck, kSpiMiso, kSpiMosi);

    FakeEsp32().gpio.attachOutput(kIli9341Cs, ili9341.cs());
    FakeEsp32().gpio.attachOutput(kIli9341Dc, ili9341.dc());
    FakeEsp32().gpio.attachOutput(kIli9341Rst, ili9341.rst());

    FakeEsp32().gpio.attachOutput(kSt7789Cs, st7789.cs());
    FakeEsp32().gpio.attachOutput(kSt7789Dc, st7789.dc());
    FakeEsp32().gpio.attachOutput(kSt7789Rst, st7789.rst());
  }
} emulator;

#endif

#include "roo_display.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/st7789.h"
#include "roo_display/shape/basic.h"

using namespace roo_display;

Ili9341spi<kIli9341Cs, kIli9341Dc, kIli9341Rst> ili9341_device;
St7789spi_240x240<kSt7789Cs, kSt7789Dc, kSt7789Rst> st7789_device;

Display ili9341_display(ili9341_device);
Display st7789_display(st7789_device);

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);

  ili9341_display.init(color::Navy);
  st7789_display.init(color::Maroon);

  DrawingContext ili9341(ili9341_display);
  ili9341.draw(FilledCircle::ByRadius(ili9341.width() / 2,
                                      ili9341.height() / 2, 80, color::Cyan));

  DrawingContext st7789(st7789_display);
  st7789.draw(FilledRect(40, 40, st7789.width() - 41,
                         st7789.height() - 41, color::Yellow));
}

void loop() {}

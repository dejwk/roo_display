// Latest results:
//
// ILI9486, SPI, 20 MHz:
// -----------------------------------------------
// Benchmark                   Time (microseconds)
// -----------------------------------------------
// Horizontal lines            113624

// ILI9431, SPI, 40 MHz:
// -----------------------------------------------
// Benchmark                   Time (microseconds)
// -----------------------------------------------
// Horizontal lines            71071
// Smooth arcs (large, round)  323637
#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/display/ili9486/ili9486spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  // FakeIli9341Spi display;
  FakeIli9486Spi display;

  Emulator() : viewport(), flexViewport(viewport, 6), display(flexViewport) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());
  }
} emulator;

#endif

#include <string>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/backlit/esp32_ledc.h"
#include "roo_display/font/font.h"
#include "roo_display/shape/smooth.h"

using namespace roo_display;

// Change these two lines to use a different driver, transport, or pins.
// #include "roo_display/driver/ili9341.h"
#include "roo_display/driver/ili9486.h"
// Ili9341spi<5, 17, 27> device;

Ili9486spi<5, 17, 27> device;

LedcBacklit backlit(16, 1);

Display display(device);

unsigned long testHorizontalLines() {
  DrawingContext dc(display);
  dc.clear();

  unsigned long start = micros();
  int16_t w = display.width();
  for (int i = 0; i < 8; i++) {
    float y = 10 + 6 * i;
    dc.draw(
        SmoothThickLine({10, y}, {w - 11, y}, (float)i * 0.5, color::Black));
  }
  for (int i = 0; i < 8; i++) {
    float y = 60.5 + 6 * i;
    dc.draw(
        SmoothThickLine({10, y}, {w - 11, y}, (float)i * 0.5, color::Black));
  }

  for (int i = 0; i < 8; i++) {
    float y = 110 + 6 * i;
    dc.draw(SmoothThickLine({10, y}, {w - 11, y}, (float)i * 0.5, color::Black,
                            ENDING_FLAT));
  }

  for (int i = 0; i < 8; i++) {
    float y = 160.5 + 6 * i;
    dc.draw(SmoothThickLine({10, y}, {w - 11, y}, (float)i * 0.5, color::Black,
                            ENDING_FLAT));
  }

  for (int i = 0; i < 8; i++) {
    float y = 210 + 6 * i;
    dc.draw(SmoothThickLine({10.5, y}, {w - 10.5, y}, (float)i * 0.5, color::Black,
                            ENDING_FLAT));
  }

  for (int i = 0; i < 8; i++) {
    float y = 260.5 + 6 * i;
    dc.draw(SmoothThickLine({10.5, y}, {w - 10.5, y}, (float)i * 0.5, color::Black,
                            ENDING_FLAT));
  }

  return micros() - start;
}

unsigned long testLargeSmoothArcs() {
  DrawingContext dc(display);
  dc.clear();

  unsigned long start = micros();

  for (int i = 0; i < 15; ++i) {
    float angle_start = -0.3f - i * 0.13f;
    float angle_end = 1.0f + i * 0.25f;
    float thickness = 20 + i * 5.0f;

    dc.draw(SmoothThickArc({160.5, 120.5}, 70, thickness, angle_start,
                           angle_end, color::BlueViolet, ENDING_ROUNDED));
  }
  return micros() - start;
}

void test() {
  Serial.println("Shapes benchmark");
  Serial.println("-----------------------------------------------");

  Serial.println("Benchmark                   Time (microseconds)");
  Serial.println("-----------------------------------------------");
  delay(10);
  Serial.print("Horizontal lines            ");
  Serial.println(testHorizontalLines());
  delay(500);
  Serial.print("Smooth arcs (large, round)  ");
  Serial.println(testLargeSmoothArcs());
  delay(500);

  Serial.println("Done!");
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::White);
  test();
}

void loop(void) {}

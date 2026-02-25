#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/display/st77xx/st77xx.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9341Spi display;

  Emulator() : viewport(), flexViewport(viewport, 1), display(flexViewport) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(2, display.dc());
    FakeEsp32().gpio.attachOutput(4, display.rst());
  }
} emulator;

#endif

// Based on Adafruit GFX demo, subject to the following copyright:

/***************************************************
  This is our GFX example for the Adafruit ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651
  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include <string>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/font/font.h"
#include "roo_display/font/font_adafruit_fixed_5x7.h"
#include "roo_display/shape/basic.h"
#include "roo_display/ui/string_printer.h"
#include "roo_display/ui/text_label.h"

using namespace roo_display;

// With clip masks, you can control very precisely which pixels of your
// drawables are actually getting drawn. And, since a clip mask is simply
// a bit-array, you can actually use Offscreen<Monochrome> to manage it.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/ili9341.h"
Ili9341spi<5, 2, 4> device;

Display display(device);
FontAdafruitFixed5x7 font;

unsigned long testFillScreen();
unsigned long testText();
unsigned long testLines(Color color);
unsigned long testFastLines(Color color1, Color color2);
unsigned long testRects(Color color);
unsigned long testFilledRects(Color color1, Color color2);
unsigned long testFilledCircles(uint8_t radius, Color color);
unsigned long testCircles(uint8_t radius, Color color);
unsigned long testTriangles();
unsigned long testFilledTriangles();
unsigned long testRoundRects();
unsigned long testFilledRoundRects();

void setup() {
  Serial.begin(9600);
  Serial.println("Adafruit_RooDisplay Test!");

  SPI.begin();
  display.init();

  Serial.println(F("Benchmark                Time (microseconds)"));
  delay(10);
  Serial.print(F("Screen fill              "));
  Serial.println(testFillScreen());
  delay(500);

  Serial.print(F("Text                     "));
  Serial.println(testText());
  delay(3000);

  Serial.print(F("Lines                    "));
  Serial.println(testLines(color::Cyan));
  delay(500);

  Serial.print(F("Horiz/Vert Lines         "));
  Serial.println(testFastLines(color::Red, color::Blue));
  delay(500);

  Serial.print(F("Rectangles (outline)     "));
  Serial.println(testRects(color::Lime));
  delay(500);

  Serial.print(F("Rectangles (filled)      "));
  Serial.println(testFilledRects(color::Yellow, color::Magenta));
  delay(500);

  Serial.print(F("Circles (filled)         "));
  Serial.println(testFilledCircles(10, color::Magenta));

  Serial.print(F("Circles (outline)        "));
  Serial.println(testCircles(10, color::White));
  delay(500);

  Serial.print(F("Triangles (outline)      "));
  Serial.println(testTriangles());
  delay(500);

  Serial.print(F("Triangles (filled)       "));
  Serial.println(testFilledTriangles());
  delay(500);

  Serial.print(F("Rounded rects (outline)  "));
  Serial.println(testRoundRects());
  delay(500);

  Serial.print(F("Rounded rects (filled)   "));
  Serial.println(testFilledRoundRects());
  delay(500);

  Serial.println(F("Done!"));
}

void loop(void) {
  for (uint8_t i = 0; i < 8; i++) {
    Orientation orientation;
    if (i & 1) orientation = orientation.swapXY();
    if (i & 2) orientation = orientation.flipHorizontally();
    if (i & 4) orientation = orientation.flipVertically();
    display.setOrientation(orientation);
    testText();
    {
      int16_t w = display.width();
      int16_t h = display.height();
      DrawingContext dc(display);
      dc.draw(Line(0, 0, w / 2 - 1, 0, color::Cyan));
      dc.draw(Line(0, 1, 0, h / 2 - 1, color::Cyan));
      dc.draw(Line(w / 2, 0, w - 1, 0, color::Magenta));
      dc.draw(Line(w - 1, 1, w - 1, h / 2 - 1, color::Magenta));
      dc.draw(Line(w / 2, h - 1, w - 1, h - 1, color::Yellow));
      dc.draw(Line(w - 1, h / 2, w - 1, h - 2, color::Yellow));
      dc.draw(Line(0, h - 1, w / 2 - 1, h - 1, color::DarkGray));
      dc.draw(Line(0, h / 2, 0, h - 2, color::DarkGray));
    }
    delay(1000);
  }
}

unsigned long testFillScreen() {
  unsigned long start = micros();
  DrawingContext dc(display);
  dc.fill(color::Black);
  yield();
  dc.fill(color::Red);
  yield();
  dc.fill(color::Lime);
  yield();
  dc.fill(color::Blue);
  yield();
  dc.fill(color::Black);
  yield();
  return micros() - start;
}

class ScreenPrinter {
 public:
  ScreenPrinter(Display& display)
      : display_(display), x_(0), y_(0), scale_(1) {}
  void setCursor(int16_t x, int16_t y) {
    x_ = x;
    y_ = y;
  }
  void println(const std::string& s) {
    DrawingContext dc(display);
    dc.setTransformation(Transformation()
                             .translate(0, font.metrics().glyphYMax())
                             .scale(scale_, scale_)
                             .translate(x_, y_));
    dc.draw(StringViewLabel(s, font, color_));
    y_ += font.metrics().linespace() * scale_;
  }
  void println(double d) {
    std::string s;
    s += d;
    println(s);
  }
  void println() { println(""); }
  void setTextColor(Color color) { color_ = color; }
  void setTextSize(int16_t size) { scale_ = size; }

 private:
  Display& display_;
  int16_t x_, y_;
  int16_t scale_;
  Color color_;
};

unsigned long testText() {
  DrawingContext dc(display);
  dc.fill(color::Black);
  ScreenPrinter printer(display);
  unsigned long start = micros();
  printer.setCursor(0, 0);
  printer.setTextColor(color::White);
  printer.setTextSize(1);
  printer.println("Hello World!");
  printer.setTextColor(color::Yellow);
  printer.setTextSize(2);
  printer.println(1234.56);
  printer.setTextColor(color::Red);
  printer.setTextSize(3);
  StringPrinter p;
  p.print(0xDEADBEEF, HEX);
  printer.println(p.get());
  printer.println();
  printer.setTextColor(color::Lime);
  printer.setTextSize(5);
  printer.println("Groop");
  printer.setTextSize(2);
  printer.println("I implore thee,");
  printer.setTextSize(1);
  printer.println("my foonting turlingdromes.");
  printer.println("And hooptiously drangle me");
  printer.println("with crinkly bindlewurdles,");
  printer.println("Or I will rend thee");
  printer.println("in the gobberwarts");
  printer.println("with my blurglecruncheon,");
  printer.println("see if I don't!");
  return micros() - start;
}

unsigned long testLines(Color color) {
  unsigned long start, t;
  int x1, y1, x2, y2, w = display.width(), h = display.height();

  DrawingContext dc(display);
  dc.fill(color::Black);
  yield();

  x1 = y1 = 0;
  y2 = h - 1;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  x2 = w - 1;
  for (y2 = 0; y2 < h; y2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  t = micros() - start;  // fillScreen doesn't count against timing

  yield();
  dc.fill(color::Black);
  yield();

  x1 = w - 1;
  y1 = 0;
  y2 = h - 1;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  x2 = 0;
  for (y2 = 0; y2 < h; y2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  t += micros() - start;

  yield();
  dc.fill(color::Black);
  yield();

  x1 = 0;
  y1 = h - 1;
  y2 = 0;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  x2 = w - 1;
  for (y2 = 0; y2 < h; y2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  t += micros() - start;

  yield();
  dc.fill(color::Black);
  yield();

  x1 = w - 1;
  y1 = h - 1;
  y2 = 0;
  start = micros();
  for (x2 = 0; x2 < w; x2 += 6) dc.draw(Line(x1, y1, x2, y2, color));
  x2 = 0;
  for (y2 = 0; y2 < h; y2 += 6) dc.draw(Line(x1, y1, x2, y2, color));

  yield();
  return micros() - start;
}

unsigned long testFastLines(Color color1, Color color2) {
  unsigned long start;
  int x, y, w = display.width(), h = display.height();

  DrawingContext dc(display);
  dc.fill(color::Black);
  start = micros();
  for (y = 0; y < h; y += 5) dc.draw(Line(0, y, w - 1, y, color1));
  for (x = 0; x < w; x += 5) dc.draw(Line(x, 0, x, h - 1, color2));

  return micros() - start;
}

unsigned long testRects(Color color) {
  unsigned long start;
  int n, i, i2, cx = display.width() / 2, cy = display.height() / 2;

  DrawingContext dc(display);
  dc.fill(color::Black);
  n = min(display.width(), display.height());
  start = micros();
  for (i = 2; i < n; i += 6) {
    i2 = i / 2;
    dc.draw(Rect(cx - i2, cy - i2, cx - i2 + i - 1, cy - i2 + i - 1, color));
  }

  return micros() - start;
}

unsigned long testFilledRects(Color color1, Color color2) {
  unsigned long start, t = 0;
  int n, i, i2, cx = display.width() / 2 - 1, cy = display.height() / 2 - 1;

  DrawingContext dc(display);
  dc.fill(color::Black);
  n = min(display.width(), display.height());
  for (i = n; i > 0; i -= 6) {
    i2 = i / 2;
    start = micros();
    dc.draw(
        FilledRect(cx - i2, cy - i2, cx - i2 + i - 1, cy - i2 + i - 1, color1));
    t += micros() - start;
    // Outlines are not included in timing results
    dc.draw(Rect(cx - i2, cy - i2, cx - i2 + i - 1, cy - i2 + i - 1, color2));
    yield();
  }

  return t;
}

unsigned long testFilledCircles(uint8_t radius, Color color) {
  unsigned long start;
  int x, y, w = display.width(), h = display.height(), r2 = radius * 2;

  DrawingContext dc(display);
  dc.fill(color::Black);
  start = micros();
  for (x = radius; x < w; x += r2) {
    for (y = radius; y < h; y += r2) {
      dc.draw(FilledCircle::ByRadius(x, y, radius, color));
    }
  }

  return micros() - start;
}

unsigned long testCircles(uint8_t radius, Color color) {
  unsigned long start;
  int x, y, r2 = radius * 2, w = display.width() + radius,
            h = display.height() + radius;

  DrawingContext dc(display);
  // Screen is not cleared for this one -- this is
  // intentional and does not affect the reported time.
  start = micros();
  for (x = 0; x < w; x += r2) {
    for (y = 0; y < h; y += r2) {
      dc.draw(Circle::ByRadius(x, y, radius, color));
    }
  }

  return micros() - start;
}

unsigned long testTriangles() {
  unsigned long start;
  int n, i, cx = display.width() / 2 - 1, cy = display.height() / 2 - 1;

  DrawingContext dc(display);
  dc.fill(color::Black);
  n = min(cx, cy);
  start = micros();
  for (i = 0; i < n; i += 5) {
    dc.draw(Triangle(cx, cy - i,      // peak
                     cx - i, cy + i,  // bottom left
                     cx + i, cy + i,  // bottom right
                     Color(i, i, i)));
  }

  return micros() - start;
}

unsigned long testFilledTriangles() {
  unsigned long start, t = 0;
  int i, cx = display.width() / 2 - 1, cy = display.height() / 2 - 1;

  DrawingContext dc(display);
  dc.fill(color::Black);
  start = micros();
  for (i = min(cx, cy); i > 10; i -= 5) {
    start = micros();
    dc.draw(FilledTriangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
                           Color(0, i * 10, i * 10)));
    t += micros() - start;
    dc.draw(Triangle(cx, cy - i, cx - i, cy + i, cx + i, cy + i,
                     Color(i * 10, i * 10, 0)));
    yield();
  }

  return t;
}

unsigned long testRoundRects() {
  unsigned long start;
  int w, i, i2, cx = display.width() / 2 - 1, cy = display.height() / 2 - 1;

  DrawingContext dc(display);
  dc.fill(color::Black);
  w = min(display.width(), display.height());
  start = micros();
  for (i = 0; i < w; i += 6) {
    i2 = i / 2;
    dc.draw(RoundRect(cx - i2, cy - i2, cx - i2 + i - 1, cy - i2 + i - 1, i / 8,
                      Color(i, 0, 0)));
  }

  return micros() - start;
}

unsigned long testFilledRoundRects() {
  unsigned long start;
  int i, i2, cx = display.width() / 2 - 1, cy = display.height() / 2 - 1;

  DrawingContext dc(display);
  dc.fill(color::Black);
  start = micros();
  for (i = min(display.width(), display.height()); i > 20; i -= 6) {
    i2 = i / 2;
    dc.draw(FilledRoundRect(cx - i2, cy - i2, cx - i2 + i - 1, cy - i2 + i - 1,
                            i / 8, Color(0, i, 0)));
    yield();
  }

  return micros() - start;
}
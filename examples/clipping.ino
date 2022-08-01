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
#include "roo_display/core/offscreen.h"
#include "roo_display/filter/clip_mask.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_Italic/90.h"

using namespace roo_display;

// With clip masks, you can control very precisely which pixels of your
// drawables are actually getting drawn. And, since a clip mask is simply
// a bit-array, you can actually use Offscreen<Monochrome> to manage it.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h"
St7789spi_240x240<5, 2, 4> device;

Display display(device);

size_t clipmask_size;
uint8_t* clipmask;

void setup() {
  Serial.begin(9600);
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init(color::LightGray);
  if (display.height() > display.width()) {
    display.setOrientation(Orientation().rotateRight());
  }
  // We allocate the bit_mask buffer dynamically here, rather than statically,
  // because we want to tie its size to the actual display size. We're using a
  // singleton b/c we don't need to use multiple bitmasks in this demo.
  clipmask_size = ((display.width() + 7) / 8) * display.height();
  clipmask = new uint8_t[clipmask_size];
}

void basicBitPatterns1() {
  long start = millis();
  DrawingContext dc(display);
  dc.fill(color::LightGray);
  // ClipMask can be narrowed by a box rectangle. Here, we're just using full
  // screen.
  ClipMask mask(clipmask, display.extents());
  dc.setClipMask(&mask);
  memset(clipmask, 0xCC, clipmask_size);
  dc.draw(
      FilledCircle::ByRadius(0, 0, display.height() / 2 - 10, color::DarkRed),
      display.width() / 2, display.height() / 2 + 5);
  memset(clipmask, 0x33, clipmask_size);
  dc.draw(
      FilledCircle::ByRadius(0, 0, display.height() / 2 - 10, color::Yellow),
      display.width() / 2, display.height() / 2 - 5);
  Serial.printf("Basic bit patterns (slow version): %ld ms\n",
                millis() - start);
}

void basicBitPatterns2() {
  long start = millis();
  // This is almost exactly like above, except for one small detail: we're
  // drawing the circles 'vertically' instead of 'horizontally'. (Not much
  // of a difference for a circle, isn't it?) But, because the clip mask is
  // vertical stripes, this trick allows the underlying driver to draw the
  // picture much faster, since it does not need to send as many window change
  // commands.
  DrawingContext dc(display);
  dc.fill(color::LightGray);
  ClipMask mask(clipmask, display.extents());
  dc.setClipMask(&mask);
  // This is where the magic happens.
  dc.setTransform(Transform().rotateRight());
  memset(clipmask, 0xCC, clipmask_size);
  dc.draw(
      FilledCircle::ByRadius(0, 0, display.height() / 2 - 10, color::DarkRed),
      display.width() / 2, display.height() / 2 + 5);
  memset(clipmask, 0x33, clipmask_size);
  dc.draw(
      FilledCircle::ByRadius(0, 0, display.height() / 2 - 10, color::Yellow),
      display.width() / 2, display.height() / 2 - 5);
  Serial.printf("Basic bit patterns (fast version): %ld ms\n",
                millis() - start);
}

constexpr double pi() { return std::atan(1) * 4; }
static const float degToRad = 2 * pi() / 360.0;

void polarToCartesian(float angle, int16_t radius, int16_t* x, int16_t* y) {
  *x = (int16_t)(radius * sin(angle * degToRad));
  *y = -(int16_t)(radius * cos(angle * degToRad));
}

void fillMask(float angleStart, float angleEnd, int16_t radius) {
  Offscreen<Monochrome> offscreen(display.width(), display.height(), clipmask,
                                  Monochrome(color::White, color::Transparent));
  int16_t quadrantStart = (int16_t)(angleStart / 90);
  int16_t quadrantEnd = (int16_t)(angleEnd / 90);
  DrawingContext odc(offscreen);
  odc.fill(color::White);
  int16_t x1, y1, x2, y2;
  polarToCartesian(angleStart, 2 * radius, &x1, &y1);
  while (quadrantStart < quadrantEnd) {
    polarToCartesian((quadrantStart + 1) * 90, 2 * radius, &x2, &y2);
    odc.erase(FilledTriangle(0, 0, x1, y1, x2, y2, color::Black),
              display.width() / 2, display.height() / 2);
    quadrantStart++;
    x1 = x2;
    y1 = y2;
  }
  // Simple case; just draw the triangle and be done with it.
  polarToCartesian(angleEnd, 2 * radius, &x2, &y2);
  odc.erase(FilledTriangle(0, 0, x1, y1, x2, y2, color::Black),
            display.width() / 2, display.height() / 2);
  return;
}

void pieChart() {
  {
    DrawingContext dc(display);
    dc.fill(color::LightGray);
  }
  // We can draw graphic primitives to the clip mask, using Offscreen. Let's
  // exploit that to draw a simple pie chart. It's done by clip-masking circle
  // using triangles.
  float angles[] = {120.0, 200.0, 260.0, 300.0, 335.0, 360.0};
  Color colors[] = {color::Red,    color::Yellow, color::Green, color::Orange,
                    color::Violet, color::Aqua,   color::Pink};
  float* nextAngle = angles;
  Color* nextColor = colors;
  int16_t radius = (int16_t)display.height() * 0.4;
  float angle1 = 0.0, angle2;
  while (true) {
    angle2 = *nextAngle;
    fillMask(angle1, angle2, radius);
    DrawingContext dc(display);
    ClipMask mask(clipmask, display.extents());
    dc.setClipMask(&mask);
    dc.draw(FilledCircle::ByRadius(display.width() / 2, display.height() / 2,
                                   radius, *nextColor++));
    angle1 = angle2;
    if (angle2 >= 360) break;
    ++nextAngle;
  }
}

// Note: graphic primitives are small objects, and it's quite OK to pass them
// by value, particularly that the compiler can optimize most copying away.
TileOf<StringViewLabel> centeredStringViewLabel(
    StringView content, Color color, Color bgcolor = color::Transparent) {
  return MakeTileOf(StringViewLabel(font_NotoSerif_Italic_90(), content, color),
                    display.extents(), HAlign::Center(), VAlign::Middle(),
                    bgcolor);
}

void clippedFont1() {
  Offscreen<Monochrome> offscreen(display.width(), display.height(), clipmask,
                                  Monochrome(color::White, color::Transparent));
  DrawingContext odc(offscreen);
  odc.fill(color::White);
  odc.erase(FilledCircle::ByRadius(display.width() / 2, display.height() / 2,
                                   50, color::Black));

  DrawingContext dc(display);
  dc.fill(color::LightGray);
  ClipMask mask(clipmask, display.extents());
  dc.setClipMask(&mask);
  dc.draw(centeredStringViewLabel("JF", color::Black, color::LightBlue));
}

void clippedFont2() {
  Offscreen<Monochrome> offscreen(display.width(), display.height(), clipmask,
                                  Monochrome(color::Black, color::White));

  DrawingContext dc(display);
  dc.fill(color::LightGray);
  ClipMask mask(clipmask, display.extents());
  dc.setClipMask(&mask);
  {
    DrawingContext odc(offscreen);
    odc.fill(color::White);
    for (int i = 0; i < display.height(); i += 4) {
      odc.draw(FilledRect(0, i, display.width() - 1, i + 1, color::Black));
    }
  }
  dc.draw(centeredStringViewLabel("Ostendo", color::Black));
  {
    DrawingContext odc(offscreen);
    odc.fill(color::White);
    for (int i = 2; i < display.height(); i += 4) {
      odc.draw(FilledRect(0, i, display.width() - 1, i + 1, color::Black));
    }
  }
  dc.draw(centeredStringViewLabel("Ostendo", color::Red));
}

void loop() {
  basicBitPatterns1();
  delay(2000);
  basicBitPatterns2();
  delay(2000);
  pieChart();
  delay(2000);
  clippedFont1();
  delay(2000);
  clippedFont2();
  delay(2000);
}

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
#include "roo_display/core/offscreen.h"
#include "roo_display/internal/raw_streamable_overlay.h"
#include "roo_display/font/font.h"
#include "roo_display/shape/basic_shapes.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_Italic/60.h"

using namespace roo_display;

// This example showcases offscreen drawing capabilities.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h"
St7789spi_240x240<5, 2, 4> device;

Display display(device);

// Offscreen implements the device interface. You can draw to it exactly how
// you would draw to a real physical device.
//
// Offscreen is very configurable:
// * it can use any of the supported color modes (ARGB8888, RGB565,
//   Monochrome, etc.)
// * You can even control byte and bit order of the physical data.
//
// Offscreen implements Drawable, so you can draw its content to the screen
// (or to another offscreen) just as if it was any other object.
//
// Offscreen exposes the underlying raster via the Streamable template
// contract (see streamable.h). What this means is that you can dynamically
// overlay offscreens with each other, without having to allocate additional
// buffers.
//
// The buffer data for the offscreen can be allocated externally (e.g.,
// statically) and provided in the constructor. Alternatively, offscreen can
// allocate content automatically on the heap.

// In these examples, we will use a large pre-drawn background buffer, and
// smaller ad-hoc dynamically allocated buffers.

static const int16_t kBgWidth = 240;
static const int16_t kBgHeight = 200;

uint8_t background_buf[kBgWidth * kBgHeight * 2];
Offscreen<Rgb565> background(kBgWidth, kBgHeight, background_buf);

void setup() {
  srand(time(nullptr));
  Serial.begin(9600);
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init(color::White);
  if (display.height() > display.width()) {
    display.setOrientation(Orientation().rotateRight());
  }
}

void fillBackground() {
  // Draw something interesting to the background buffer.
  DrawingContext dc(background);
  dc.fill(color::Bisque);

  for (int i = 0; i < 500; i++) {
    Color color(rand(), rand(), rand());
    // We want 'light' pastel colors. We can achieve that by alpha-blending
    // them over white background, using some level of transparency.
    color.set_a(0x40);
    color = alphaBlend(Graylevel(0xE0), color);
    int16_t x = rand() % kBgWidth;
    int16_t y = rand() % kBgHeight;
    int16_t r = rand() % (kBgWidth / 3);
    dc.draw(FilledCircle::ByRadius(x, y, r, color));
  }
}

void drawImageCentered() {
  DrawingContext dc(display);
  dc.fill(color::Gray);
  dc.draw(background, (display.width() - kBgWidth) / 2,
          (display.height() - kBgHeight) / 2);
}

struct Point {
  int16_t x;
  int16_t y;
};

Point calculateSpiralXY(double time) {
  double r = 300 * pow((time + 2), -1.2);
  Point p;
  p.x = (int16_t)(r * sin(time));
  p.y = (int16_t)(r * cos(time));
  return p;
}

void someFunWithAntiAliasedFonts() {
  StringViewLabel qa("Ostendo", font_NotoSerif_Italic_60(), color::Black);
  // Now, we make a memory copy of the rendered label. By using Alpha4 with
  // no background, we only use half-byte per pixel, yet we wan apply
  // arbitrary colors to the text and still enjoy anti-aliasing.
  Offscreen<Alpha4> oqa(qa, Alpha4(color::Black));

  // Now, we're making two other versions of this text, using different colors,
  // but sharing the same underlying memory buffer.
  Color shadow1 =
      alphaBlend(Graylevel(0x20), Color(0xE0, rand(), rand(), rand()));
  Color shadow2 =
      alphaBlend(Graylevel(0x20), Color(0xE0, rand(), rand(), rand()));

  Offscreen<Alpha4> oqa_s1(oqa.extents(), oqa.buffer(), Alpha4(shadow1));
  Offscreen<Alpha4> oqa_s2(oqa.extents(), oqa.buffer(), Alpha4(shadow2));

  // Now, we will draw them all anti-aliased on top of each other,
  // and on top of the background, without needing any more memory.
  Box bounds = background.extents();
  int16_t xCenter = (kBgWidth - qa.extents().width()) / 2;
  int16_t yCenter = (kBgHeight - qa.extents().height()) / 2;
  int16_t xMin = (display.width() - kBgWidth) / 2;
  int16_t yMin = (display.height() - kBgHeight) / 2;
  for (int i = 0; i < 280; i++) {
    Point p0 = calculateSpiralXY(i / 10.0);
    Point p1 = calculateSpiralXY(i / 10.0 + 1);
    Point p2 = calculateSpiralXY(i / 10.0 + 2);
    auto text = Overlay(oqa_s2.raster(), xCenter + p0.x, yCenter + p0.y,
                        Overlay(oqa_s1.raster(), xCenter + p1.x, yCenter + p1.y,
                                oqa.raster(), xCenter + p2.x, yCenter + p2.y),
                        0, 0);
    auto result = MakeDrawableRawStreamable(Overlay(
        Clipped(Box::extent(bounds, text.extents()), background.raster()), 0, 0,
        std::move(text), 0, 0));
    bounds = text.extents();
    {
      DrawingContext dc(display);
      dc.setClipBox(xMin, yMin, xMin + kBgWidth - 1, yMin + kBgHeight - 1);
      dc.draw(result, xMin, yMin);
    }
  }

  Point p0 = calculateSpiralXY(28);
  Point p1 = calculateSpiralXY(29);
  Point p2 = calculateSpiralXY(30);
  auto text = Overlay(oqa_s2.raster(), xCenter + p0.x, yCenter + p0.y,
                      Overlay(oqa_s1.raster(), xCenter + p1.x, yCenter + p1.y,
                              oqa.raster(), xCenter + p2.x, yCenter + p2.y),
                      0, 0);
  auto result = MakeDrawableRawStreamable(
      Overlay(background.raster(), 0, 0, std::move(text), 0, 0));
  for (int i = 5; i < 30; ++i) {
    int scale = (int)pow(1.2, i);
    DrawingContext dc(display);
    dc.setTransform(
        // Rescale the content rectangle by its center, and then move it
        // to the center of the screen.
        Transform()
            .translate(-kBgWidth / 2, -kBgHeight / 2)
            .scale(scale, scale)
            .translate(display.width() / 2, display.height() / 2));
    dc.draw(result);
    delay(50);
  }

  // The 'overlay' trick has limitations: it doesn't work with transformations,
  // and it eats up stack RAM quite quickly so it can't be used for more than a
  // few overlays at a time.
}

void loop(void) {
  fillBackground();
  drawImageCentered();
  delay(1000);
  someFunWithAntiAliasedFonts();
  delay(1000);
}

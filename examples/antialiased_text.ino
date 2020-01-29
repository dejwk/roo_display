#include <string>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSerif_Italic/12.h"
#include "roo_smooth_fonts/NotoSerif_Italic/27.h"

using namespace roo_display;

// Anti-aliased text is simple if you're using a solid background. Just
// set the bgcolor on the DrawingContext, and prosper!
//
// If you want to use non-trivial backgrounds (color gradients, or backgorund
// images), the story is much more complicated. Anti-aliasing requires alpha-
// blending, which mixes up background colors with the foreground. It
// requires being able to read from the device's framebuffer, which is often
// not supported by the hardware, or buffering in RAM, which is often too
// expensive (480x320 image with a typical RGB565 color mode needs 300KB,
// which is more than ESP32 has to spare).
//
// This example shows what happens when anti-aliasing goes wrong, and shows
// various options to get it right, from solid backgrounds to RAM buffering
// to repetitive drawing using clip regions.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h" 
St7789_240x240<13, 2, 4> device;

Display display(&device, nullptr);

void setup() {
  Serial.begin(9600);
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init(color::White);
}

// Since we're drawing the same contents in various places and scales, we
// define a helper Drawable that captures all the 'business logic' on how
// to draw it.
class Widget : public Drawable {
 public:
  Box extents() const override { return Box(0, 0, 39, 39); }
  void drawTo(const Surface& s) const override {
    s.drawObject(FilledRect(0, 0, 19, 39, color::IndianRed));
    s.drawObject(FilledRect(20, 0, 39, 39, color::LightGreen));
    s.drawObject(
        MakeTileOf(TextLabel(font_NotoSerif_Italic_27(), "FJ", color::Black),
                   Box(5, 2, 34, 37), HAlign::Center(), VAlign::Middle()));
  }
};

// This helper method takes our widget, and draws two copies of it: first
// in the top-left corner, in the 1:1 scale, and the second, magnified as
// much as possible to fit on screen, in the middle of the remaining area.
void printText(DrawingContext& dc, const Drawable& widget) {
  // First, draw the image in the top-left corner.
  dc.draw(widget);

  // Now, draw the maximally zoomed-in image in the middle of the remaining
  // screen space.
  int16_t w = display.width();
  int16_t h = display.height();
  int16_t maxVzoom = w / 40 - 1;
  int16_t maxHzoom = h / 40 - 1;
  int16_t zoom = maxVzoom < maxHzoom ? maxVzoom : maxHzoom;
  if (zoom < 1) zoom = 1;
  dc.setTransform(Transform()
                      .translate(-20, -20)  // Bring the origin to the middle.
                      .scale(zoom, zoom)    // Rescale about the middle.
                      .translate(20 + w / 2,
                                 20 + h / 2));  // Move to the suitable place.
  dc.draw(widget);
}

// Use the device's 'default' background color, and write the text as
// transparent.
void printTransparentlyUsingDeviceBackground(Color bgcolor) {
  Serial.println("Drawing with device background.");
  DrawingContext dc(&display);
  device.setBgColorHint(bgcolor);
  printText(dc, Widget());
}

// Draws the glyphs using a solid background (overwriting previous background).
void printUsingSolidBackground(Color bgcolor) {
  Serial.println("Drawing with solid background.");
  DrawingContext dc(&display);
  dc.setBgColor(bgcolor);
  printText(dc, Widget());
}

void printUsingRamBuffer() {
  Serial.println("Drawing using RAM buffer. This should look great overall.");
  // This constructor allocates memory on the heap. The heap allocation has
  // lots of bad press, but it's actually just fine for temporary objects
  // that get deleted before anything else is allocated on the heap.
  // As an alternative, OffscreenDisplay (and Offscreen) can take pre-allocated
  // buffer in the constructor.
  OffscreenDisplay<Rgb565> offscreen(40, 40);
  {
    // It is important to ensure the drawing context gets destroyed before using
    // the content. (It is achieved here by enclosing it in a nested bock).
    // Otherwise, in some circumstances you can see 'unfinished' results, since
    // the underlying implementation has liberty to use buffered writes.
    DrawingContext dc(&offscreen);
    dc.draw(Widget());
  }
  // Now, we can use the offscreen as a regular drawable.
  DrawingContext dc(&display);
  printText(dc, offscreen);
}

void printUsingClipping() {
  Serial.println(
      "Drawing two halves separately, and clipping. This should look "
      "great overall. Might be tad slower than via RAM buffer.");
  DrawingContext dc(&display);
  // Print the left half.
  dc.setBgColor(color::IndianRed);
  // Clipped
  Widget widget;
  printText(dc,
            TransformedDrawable(Transform().clip(Box(0, 0, 19, 39)), &widget));
  dc.setBgColor(color::LightGreen);
  printText(dc,
            TransformedDrawable(Transform().clip(Box(20, 0, 39, 39)), &widget));
}

void loop() {
  // If you have enough RAM, you can use an extra buffer to pre-render the
  // image. This solves all alpha problems.
  printUsingRamBuffer();
  delay(8000);

  // Now let's see how anti-aliasing can go wrong.

  // This would have looked good on white background, but it does not look good
  // on either red or green.
  printTransparentlyUsingDeviceBackground(color::White);
  delay(2000);

  // Using the same bg and fg colors disables anti-aliasing completely.
  printTransparentlyUsingDeviceBackground(color::Black);
  delay(2000);

  // Being somewhat 'in-between', Gray looks 'ok-ish' but not great on neither
  // red nor green.
  printTransparentlyUsingDeviceBackground(color::Gray);
  delay(2000);

  // Using red makes the font look great on the red background, but horrible on
  // the green background.
  printTransparentlyUsingDeviceBackground(color::IndianRed);
  delay(2000);

  // Using red makes the font look great on the red background, but horrible on
  // the green background.
  printTransparentlyUsingDeviceBackground(color::LightGreen);
  delay(2000);

  // Using a solid background will make the edges look great, but it overwrites
  // the original background so it is not always appropriate.
  printUsingSolidBackground(color::White);
  delay(2000);
  printUsingSolidBackground(color::Gray);
  delay(2000);
  printUsingSolidBackground(color::IndianRed);
  delay(2000);
  printUsingSolidBackground(color::LightGreen);
  delay(2000);

  // Now, let's see a potential solution that works well if you don't
  // have enough memory to spare, and the background isn't too complicated.

  // The trick we use here is to use clipping to drive the relevant parts
  // of the string independently on each background.
  printUsingClipping();
  delay(8000);
}

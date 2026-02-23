// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#basic-setup

#include "Arduino.h"
#include "roo_display.h"

using namespace roo_display;

// Select the driver to match your display device.
#include "roo_display/driver/ili9341.h"
#include "roo_display/driver/st7789.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_display/products/lilygo/t_display_s3.h"

// Set your configuration for the driver.
static constexpr int kCsPin = 7;
static constexpr int kDcPin = 2;
static constexpr int kRstPin = 3;
static constexpr int kBlPin = 20;
static constexpr int kTouchCsPin = 1;

static constexpr int kSpiSck = 4;
static constexpr int kSpiMiso = 5;
static constexpr int kSpiMosi = 6;

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin);

St7796spi_320x480<kCsPin, kDcPin, kRstPin> device;
Display display(device);

#include <string>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/color/gradient.h"
#include "roo_display/ui/text_label.h"
#include "roo_display/ui/tile.h"
// Needs "dejwk/roo_fonts_basic".
#include "roo_fonts/NotoSans_Condensed/15.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  // device.initTransport();
  display.init(Graylevel(0xF0));

  //   // Uncomment if using backlit.
  //   backlit.begin();
}

void DrawRgbGradient(DrawingContext& dc, Color start, Color end, Color border,
                     int border_width) {
  int w = dc.width();
  int h = dc.height();
  Box inner_box(0, 0, w - 2 * border_width - 1, h - 2 * border_width - 1);
  ColorGradient gradient({{0.0f, start}, {1.0f, end}});
  dc.draw(MakeTileOf(VerticalGradient(0, 1.0f / w, gradient, inner_box),
                     dc.bounds(), kCenter | kMiddle, border));
}

void loop() {
  int w = display.width();
  int h = display.height();
  const auto& font = font_NotoSans_Condensed_15();
  int text_height = font.metrics().linespace();
  {
    DrawingContext dc(display);
    dc.draw(StringViewLabel("Color test", font, color::Black), kCenter | kTop);
  }
  int gradient_h = (h - text_height) / 14;
  int border_w = (h - text_height) / 60;
  int current_h = text_height;
  Box gradient_bounds(0, 0, w - 1, gradient_h - 1);

  // Draw gradients to black.
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Red, color::Black, color::Black, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Green, color::Black, color::Black, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Blue, color::Black, color::Black, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Cyan, color::Black, color::Black, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Magenta, color::Black, color::Black, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Yellow, color::Black, color::Black, border_w);
  }
  current_h += gradient_h;

  // Draw gradients to white.
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Red, color::White, color::White, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Green, color::White, color::White, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Blue, color::White, color::White, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Cyan, color::White, color::White, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Magenta, color::White, color::White, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Yellow, color::White, color::White, border_w);
  }
  current_h += gradient_h;

  // Monochrome gradients.
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Black, color::White, color::White, border_w);
  }
  current_h += gradient_h;
  {
    DrawingContext dc(display, 0, current_h, gradient_bounds);
    DrawRgbGradient(dc, color::Black, color::White, color::Black, border_w);
  }
  current_h += gradient_h;

  delay(10000);
}
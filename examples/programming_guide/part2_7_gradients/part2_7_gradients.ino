// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#gradients

#include "Arduino.h"
#include "roo_display.h"

using namespace roo_display;

// Select the driver to match your display device.
#include "roo_display/driver/ili9341.h"

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 2;
static constexpr int kRstPin = 4;
static constexpr int kBlPin = 16;

// Uncomment if you have connected the BL pin to GPIO.

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin, /* ledc channel */ 0);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

#include "roo_display/color/gradient.h"
#include "roo_display/color/hsv.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/90.h"

void setup() {
  SPI.begin();
  display.init(Graylevel(0xF0));
}

void loop() {
  int w = display.width();
  int h = display.height();
  auto centered = kMiddle | kCenter;
  {
    // Radial gradient.
    DrawingContext dc(display, Box(0, 0, w / 2 - 1, h / 2 - 1));
    auto gradient = RadialGradientSq(
        {w / 4, h / 4}, ColorGradient({{0, HsvToRgb(60, 0.8, 0.99)},
                                       {50 * 50, HsvToRgb(0, 0.8, 0.95)},
                                       {51 * 51, color::Transparent}}));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }
  {
    // Periodic, angular gradient.
    // Note that we're repeating the same color at both ends of the gradient,
    // to make the period equal to 120 degrees (2 * Pi / 3) and to smoothly
    // oscillate between red and yellow.
    DrawingContext dc(display, Box(w / 2, 0, w - 1, h / 2 - 1));
    auto gradient =
        AngularGradient({w * 3 / 4, h / 4},
                        ColorGradient({{0, HsvToRgb(60, 0.8, 0.95)},
                                       {M_PI / 3, HsvToRgb(0, 0.8, 0.99)},
                                       {M_PI / 1.5, HsvToRgb(60, 0.8, 0.95)}},
                                      ColorGradient::PERIODIC),
                        Box(w / 2 + 20, 20, w - 21, h / 2 - 21));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }
  {
    // Multi-node vertical gradient.
    DrawingContext dc(display, Box(0, h / 2, w / 2 - 1, h - 1));
    float v = 0.9;
    float s = 0.7;
    auto gradient = VerticalGradient(10, 1,
                                     ColorGradient({{0, HsvToRgb(0, s, v)},
                                                    {20, HsvToRgb(60, s, v)},
                                                    {40, HsvToRgb(120, s, v)},
                                                    {60, HsvToRgb(180, s, v)},
                                                    {80, HsvToRgb(240, s, v)},
                                                    {100, HsvToRgb(300, s, v)},
                                                    {120, HsvToRgb(360, s, v)}},
                                                   ColorGradient::PERIODIC),
                                     Box(20, h / 2 + 20, w / 2 - 21, h - 21));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }
  {
    // Multi-node skewed linear gradient.
    DrawingContext dc(display, Box(w / 2, h / 2, w - 1, h - 1));
    float v = 0.9;
    float s = 0.7;
    auto gradient = LinearGradient({w / 2 + 10, h / 2 + 10}, 0.5, 0.3,
                                   ColorGradient({{0, HsvToRgb(0, s, v)},
                                                  {20, HsvToRgb(60, s, v)},
                                                  {40, HsvToRgb(120, s, v)},
                                                  {60, HsvToRgb(180, s, v)},
                                                  {80, HsvToRgb(240, s, v)},
                                                  {100, HsvToRgb(300, s, v)},
                                                  {120, HsvToRgb(360, s, v)}},
                                                 ColorGradient::PERIODIC),
                                   Box(w / 2 + 20, h / 2 + 20, w - 21, h - 21));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
            kCenter | kMiddle);
  }

  delay(10000);
}
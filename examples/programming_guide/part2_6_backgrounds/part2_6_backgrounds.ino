// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#backgrounds

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

static constexpr int kSpiSck = -1;
static constexpr int kSpiMiso = -1;
static constexpr int kSpiMosi = -1;

// Uncomment if you have connected the BL pin to GPIO.

// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

#include "roo_display/color/hsv.h"
#include "roo_display/core/raster.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/90.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(Graylevel(0xF0));

  // Uncomment if using backlit.
  // backlit.begin();
}

void vertical() {
  // 1x240 Rgb565 pixels.
  static uint8_t background_data[240 * 2];

  {
    Offscreen<Rgb565> offscreen(1, 240, background_data);
    DrawingContext dc(offscreen);
    dc.drawPixels([](PixelWriter& writer) {
      for (int i = 0; i < 240; ++i) {
        writer.writePixel(0, i, HsvToRgb(360.0 / 240 * i, 0.5, 0.9));
      }
    });
  }

  display.clear();

  DrawingContext dc(display);
  // Creates a single vertical line raster.
  ConstDramRaster<Rgb565> raster(1, 240, background_data);
  // Repeats the raster pattern infinitely.
  auto bg = MakeTiledRaster(&raster);
  dc.setBackground(&bg);
  // Actually fill the screen with the background.
  // (Alternatively, experiment with kFillRectangle).
  dc.clear();
  // Magnify to emphasize anti-aliasing.
  dc.setTransformation(Transformation().scale(3, 3));
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);

  delay(2000);
}

void checkers() {
  display.clear();

  DrawingContext dc(display);
  auto bg = MakeRasterizable(
      Box(60, 60, 259, 179),
      [](int16_t x, int16_t y) {
        return ((x / 40) - (y / 40)) % 2 ? color::White : color::MistyRose;
      },
      kNoTransparency);
  dc.setBackground(&bg);
  dc.clear();

  dc.setTransformation(Transformation().scale(3, 3));
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);

  delay(2000);
}

void semi_transparent() {
  display.clear();

  DrawingContext dc(display);
  auto bg = MakeRasterizable(dc.bounds(), [](int16_t x, int16_t y) {
    return color::RoyalBlue.withA(y);
  });
  dc.setBackground(&bg);
  dc.setBackgroundColor(color::Violet);
  dc.clear();

  dc.setTransformation(Transformation().scale(5, 5));
  dc.draw(TextLabel("&", font_NotoSerif_Italic_90(), color::Black),
          kCenter | kMiddle);

  delay(2000);
}

void loop() {
  vertical();
  checkers();
  semi_transparent();
}
#include <string>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/text_label.h"
#include "roo_smooth_fonts/NotoSans_Italic/12.h"
#include "roo_smooth_fonts/NotoSans_Italic/60.h"

using namespace roo_display;

// Using setBackground, you can set (uncompressed) images, as well as
// syntethically crated backgrounds such as gradients and meshes,
// as a background for any drawing primitives (including antialiazed fonts).
// You can do it for every created DrawingContext separately, but if the
// background is static, you can also set it once on the display, and then
// DrawingContexts will inherit it.
//
// DrawingContext::clear() respects the background. In other words, if the
// background is set, DrawingContext::clear() fills the clip box with the
// background.

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/st7789.h" 
St7789spi_240x240<13, 2, 4> device;

Display display(&device, nullptr);

static const int kGradientSize = 200;
Color hsvGradient[kGradientSize];

void initGradient();

auto slantedGradient =
    MakeSynthetic(Box::MaximumBox(), [](int16_t x, int16_t y) {
      return hsvGradient[(kGradientSize + x - y / 4) % kGradientSize];
    });

void setup() {
  Serial.begin(9600);
  initGradient();
  SPI.begin();  // Use default SPI pins, or specify your own here.
  display.init();
  display.setBackground(&slantedGradient);
  {
    DrawingContext dc(&display);
    dc.clear();  // Fill the entire display with the background gradient.
  }
}

void loop() {
  auto labelOrig = TextLabel(font_NotoSans_Italic_60(), "Afy", color::Black,
                             FILL_MODE_RECTANGLE);
  auto labelScaled = TextLabel(font_NotoSans_Italic_12(), "Afy", color::Black,
                               FILL_MODE_RECTANGLE);
  int16_t dx = 10;
  int16_t dy = 10 + font_NotoSans_Italic_60().metrics().glyphYMax();
  // The 'rescaled' label is slightly larger, so we truncate to avoid artifacts
  // at the edges. (We could also do the opposite, but we would need to use
  // Tile).
  auto clip_box = labelOrig.extents().translate(dx, dy);
  {
    DrawingContext dc(&display);
    dc.setClipBox(clip_box);
    dc.draw(labelOrig, dx, dy);
  }
  delay(2000);
  {
    DrawingContext dc(&display);
    dc.setClipBox(clip_box);
    dc.setTransform(Transform().scale(5, 5));
    dc.draw(labelScaled, dx, dy);
  }
  delay(2000);
}

Color hsvToRgb(double h, double s, double v) {
  // See https://en.wikipedia.org/wiki/HSL_and_HSV.
  double c = v * s;
  double hp = h / 60.0;
  int ihp = (int)hp;
  double x = c * (1 - std::abs((hp - 2 * (ihp / 2)) - 1));
  double m = v - c;
  int ix = (int)(255 * x);
  int ic = (int)(255 * c);
  int im = (int)(255 * m);
  int ixm = ix + im;
  int icm = ic + im;
  switch (ihp % 6) {
    case 0:
      return Color(icm, ixm, im);
    case 1:
      return Color(ixm, icm, im);
    case 2:
      return Color(im, icm, ixm);
    case 3:
      return Color(im, ixm, icm);
    case 4:
      return Color(ixm, im, icm);
    case 5:
      return Color(icm, im, ixm);
    default:
      return Color(0);
  }
}

void initGradient() {
  double dh = 360.0 / kGradientSize;
  double h = 0;
  for (int i = 0; i < kGradientSize; ++i) {
    hsvGradient[i] = hsvToRgb(h, 0.5, 0.9);
    h += dh;
  }
}


// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#using-rasterizable-overlays-and-color-filters

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

#include "roo_display/filter/foreground.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Bold/27.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(color::LightSeaGreen);

  // Uncomment if using backlit.
  // backlit.begin();
}

class PressAnimationOverlay : public Drawable {
 public:
  PressAnimationOverlay(const Drawable* contents)
      : contents_(contents), xc_(0), yc_(0), r_(0) {}

  Box extents() const override { return contents_->extents(); }

  void set(int16_t xc, int16_t yc, int16_t r) {
    xc_ = xc;
    yc_ = yc;
    r_ = r;
  }

 private:
  void drawTo(const Surface& s) const override {
    Surface my_s(s);
    my_s.set_bgcolor(AlphaBlend(s.bgcolor(), color::Purple.withA(0x20)));
    auto circle = SmoothFilledCircle({xc_, yc_}, r_, color::Purple.withA(0x30));
    ForegroundFilter fg(s.out(), &circle, s.dx(), s.dy());
    if (r_ != 0) {
      my_s.set_out(&fg);
    }
    my_s.drawObject(*contents_);
  }

  const Drawable* contents_;
  int16_t xc_, yc_, r_;
};

void loop() {
  auto tile = MakeTileOf(
      TextLabel("Hello, World!", font_NotoSans_Bold_27(), color::Black),
      Box(0, 0, 200, 50), kCenter | kMiddle);
  PressAnimationOverlay overlaid(&tile);
  if ((millis() / 1000) % 2 == 0) {
    overlaid.set(120, 25, (millis() % 1000) / 2);
  }
  DrawingContext dc(display);
  dc.setFillMode(kFillRectangle);
  dc.draw(overlaid, kCenter | kMiddle);
}

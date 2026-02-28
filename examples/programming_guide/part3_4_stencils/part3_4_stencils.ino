
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

// // Uncomment if you have connected the BL pin to GPIO.
// #include "roo_display/backlit/esp32_ledc.h"
// LedcBacklit backlit(kBlPin);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

#include "roo_display/color/blending.h"
#include "roo_display/color/gradient.h"
#include "roo_display/color/hsv.h"
#include "roo_display/filter/blender.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/90.h"

auto bg = LinearGradient(
    {0, 0}, 0.0, 1.0,
    ColorGradient({{0.0, Graylevel(0xFF)},
                   {(float)display.height(), Graylevel(0x80)}},
                  ColorGradient::TRUNCATED));

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.setBackground(&bg);
  display.init(color::White);

  // // Uncomment if using backlit.
  // backlit.begin();
}

inline const Font &font() { return font_NotoSerif_Italic_90(); }

class MyLabel : public Drawable {
public:
  MyLabel(std::string text)
      : Drawable(),
        extents_(font().getHorizontalStringMetrics(text).screen_extents()),
        text_(std::move(text)) {}

  Box extents() const override { return extents_; }

  void drawTo(const Surface &surface) const override {
    float v = 0.9;
    float s = 0.7;
    float t = (float)millis() / 2000.0;
    float angle = 0.5 * sinf(t); // angle between approx. -30° - 30°.
    float dx = 0.5 * cosf(angle);
    float dy = 0.5 * sinf(angle);
    float shift = t * 100;
    auto bg = LinearGradient({0, 0}, dx, dy,
                             ColorGradient({{0 + shift, HsvToRgb(0, s, v)},
                                            {20 + shift, HsvToRgb(60, s, v)},
                                            {40 + shift, HsvToRgb(120, s, v)},
                                            {60 + shift, HsvToRgb(180, s, v)},
                                            {80 + shift, HsvToRgb(240, s, v)},
                                            {100 + shift, HsvToRgb(300, s, v)},
                                            {120 + shift, HsvToRgb(360, s, v)}},
                                           ColorGradient::PERIODIC));

    // Impose a destination-in blending filter. The destination-in causes the
    // text to be 'cut out' of the background (i.e. the gradient defined above),
    // showing the background only where the text would be.
    BlendingFilter<BlendOp<BlendingMode::kDestinationIn>> filter(surface.out(),
                                                                 &bg);
    // We set up a new surface, which mostly inherits from the original one, but
    // with the blending filter as the output device. This way, when we draw the
    // text to this surface, it will be processed by the filter, which will
    // combine it with the background gradient, and then output the final result
    // to the original surface.
    Surface news = surface;
    news.set_out(&filter);

    // Very important: we don't want to use the background inherited from the
    // original surface, which is often an opaque color hint coming from the
    // underlying device. For 'destination-in' blending, the 'source' (text in
    // this case) is used as a 'cut mask', so it is important to preserve its
    // translucency.
    news.set_bgcolor(color::Transparent);

    DrawingContext dc(news);
    dc.draw(TextLabel(text_, font(), color::Black));
  }

private:
  Box extents_;
  std::string text_;
};

void loop() {
  {
    DrawingContext dc(display);
    dc.draw(MyLabel("Hello!"), kCenter | kMiddle);
  }
  delay(1);
}
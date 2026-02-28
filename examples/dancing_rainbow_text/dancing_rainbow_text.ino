
#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9341Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(), flexViewport(viewport, 1, FlexViewport::kRotationRight),
        display(flexViewport),
        touch(flexViewport, FakeXpt2046Spi::Calibration(318, 346, 3824, 3909,
                                                        false, true, false)) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());

    FakeEsp32().attachSpiDevice(touch, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(2, touch.cs());
  }
} emulator;

#endif

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/color/gradient.h"
#include "roo_display/color/hsv.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/90.h"
#include "roo_display/shape/smooth_transformation.h"

using namespace roo_display;

// Select the driver to match your display device.
#include "roo_display/driver/ili9341.h"

// Set your configuration for the driver.
static constexpr int kCsPin = 5;
static constexpr int kDcPin = 17;
static constexpr int kRstPin = 27;
static constexpr int kBlPin = 16;

static constexpr int kSpiSck = -1;
static constexpr int kSpiMiso = -1;
static constexpr int kSpiMosi = -1;

// Uncomment if you have connected the BL pin to GPIO.
#include "roo_display/backlit/esp32_ledc.h"
LedcBacklit backlit(kBlPin);

Ili9341spi<kCsPin, kDcPin, kRstPin> device(Orientation().rotateLeft());
Display display(device);

const char *text = "Hello!";
inline const Font &font() { return font_NotoSerif_Italic_90(); }

Offscreen<Rgb565> offscreen(font().getHorizontalStringMetrics(text).screen_extents());

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(color::White);  // Speeds up full-screen updates.
  display.enableTurbo();

  // Uncomment if using backlit.
  backlit.begin();
}

class RainbowLabel : public Drawable {
public:
  RainbowLabel(std::string text)
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
                                           ColorGradient::Boundary::kPeriodic));

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

void updateRaster(bool front) {
  DrawingContext dc(offscreen);
  dc.fill(color::White);
  if (front) {
    dc.draw(RainbowLabel(text));
  } else {
    dc.draw(TextLabel(text, font(), color::Gray));
  }
}

void loop() {
  // Rotation about the 'Y' axis (3D).
  float angle = millis() / 4000.0f;
  float c = cosf(angle);
  float s = sinf(angle);
  float width = offscreen.anchorExtents().width();
  float camera_distance = std::max(width, 1.0f);
  float perspective_px = -s / camera_distance;
  updateRaster(c > 0);

  // Additional wobble around the X axis.
  float wobble_angle = 0.3f * sinf(millis() / 1000.0f);
  {
    DrawingContext dc(display);

    auto d = TransformRaster(
        offscreen.raster(),
        // Bring the center of text to the origin.
        Translate(-offscreen.anchorExtents().width() / 2.0f,
                  font().metrics().ascent() / 2.0f)
            // Apply the X 'wobble'.
            .then(Rotation(wobble_angle))
            // Apply perspective horizontally.
            .then(Perspective(perspective_px, 0.0f))
            // Also now scale horizontally, to simulate 3D rotation. The
            // perspective transformation alone would make the text trapezoidal,
            // but it doesn't squeeze it, so we need to apply an additional
            // horizontal scaling to make it look right.
            .then(Scale(c, 1.0f))
            // Move center to the middle of the screen.
            .then(Translate(display.extents().width() / 2.0f,
                            display.extents().height() / 2.0f)));
    // Using tile to make sure that all previous contents is overwritten.
    dc.draw(MakeTileOf(d, display.extents()));
  }
  delay(1);
}
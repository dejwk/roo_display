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
  display.init(color::Black);

  // Uncomment if using backlit.
  // backlit.begin();
}

class JumpingPedroSpriteOverlay : public Drawable {
 public:
  JumpingPedroSpriteOverlay(const Drawable *contents)
      : contents_(contents), xc_(0), yc_(0), jumping_(false) {}

  Box extents() const override { return contents_->extents(); }

  void set(int16_t xc, int16_t yc, bool jumping) {
    xc_ = xc;
    yc_ = yc;
    jumping_ = jumping;
  }

 private:
  void drawTo(const Surface &s) const override {
    static const uint8_t pedro_standing_data[] PROGMEM = {
        0x01, 0x00, 0x05, 0x40, 0x05, 0x40, 0x06, 0x40, 0x09, 0x80, 0x15,
        0x50, 0x15, 0x55, 0x6A, 0xAA, 0x95, 0x55, 0x5F, 0x34, 0x1F, 0xF0,
        0x0F, 0xC0, 0x65, 0x64, 0x59, 0x94, 0xD6, 0x5C, 0xFD, 0x5F, 0xFD,
        0x5F, 0x7D, 0x50, 0x55, 0x50, 0xF0, 0xF0, 0xFC, 0xFC, 0x3C, 0xFC,
    };

    static const uint8_t pedro_jumping_data[] PROGMEM = {
        0x01, 0x00, 0x05, 0x40, 0x05, 0x40, 0x06, 0x40, 0x09, 0x90, 0x05,
        0x54, 0x15, 0x54, 0x5A, 0xAA, 0x65, 0x55, 0x9F, 0x34, 0x1F, 0xF0,
        0xCF, 0xC3, 0xF9, 0x9F, 0xF6, 0x5F, 0xF5, 0x5C, 0x55, 0x50, 0x55,
        0x57, 0x15, 0x5F, 0x15, 0x3F, 0x0F, 0x0C, 0x0F, 0xC0, 0x0F, 0xC0,
    };

    static Color colors[] PROGMEM = {color::Transparent, Color(0xFF7A262C),
                                     Color(0xFFF8C02C), Color(0xFFFAA6AC)};

    static Palette palette = Palette::ReadOnly(colors, 4);

    Surface my_s(s);
    auto pedro_raster = ProgMemRaster<Indexed2>(
        8, 22, jumping_ ? pedro_jumping_data : pedro_standing_data, &palette);
    auto pedro_scaled = MakeRasterizable(
        Box(0, 0, 8 * 4 - 1, 22 * 2 - 1),
        [&](int16_t x, int16_t y) { return pedro_raster.get(x / 4, y / 2); });
    ForegroundFilter fg(s.out(), &pedro_scaled, xc_ + s.dx(), yc_ + s.dy());
    my_s.set_out(&fg);
    my_s.drawObject(*contents_);
  }

  const Drawable *contents_;
  int16_t xc_, yc_;
  bool jumping_;
};

Box invalid(display.extents());

void loop() {
  auto tile = MakeTileOf(
      TextLabel("Hello, World!", font_NotoSans_Bold_27(), color::SkyBlue),
      display.extents(), kCenter | kMiddle);
  JumpingPedroSpriteOverlay overlaid(&tile);
  int y = 140;
  bool jumping = (millis() / 500) % 4 == 0;
  if (jumping) {
    int progress = millis() % 500;
    int dy = (progress - 250) / 10;
    y = 140 - (625 - (dy * dy)) / 13;
  }
  overlaid.set(180, y, jumping);
  Box pedro = Box(0, 0, 8 * 4 - 1, 22 * 2 - 1).translate(180, y);
  invalid = Box::Extent(invalid, pedro);
  DrawingContext dc(display);
  dc.setClipBox(invalid);
  invalid = pedro;
  dc.setFillMode(FillMode::kExtents);
  dc.draw(overlaid, kCenter | kMiddle);
}
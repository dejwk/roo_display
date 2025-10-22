// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#dynamic-composition

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

#include "roo_display/color/gradient.h"
#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/shape/smooth.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(color::LightSeaGreen);

  // Uncomment if using backlit.
  // backlit.begin();
}

void rasterizable_stack_lifebuoy() {
  display.setBackgroundColor(color::LightSeaGreen);
  display.clear();

  auto circle = SmoothThickCircle({100, 100}, 71, 42, Color(0xFFEC6D44));
  auto rect1 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, M_PI / 4, color::White);
  auto rect2 =
      SmoothRotatedFilledRect({100, 100}, 40, 200, -M_PI / 4, color::White);
  auto gradient = RadialGradientSq(
      {100, 100}, ColorGradient({{50 * 50, color::Black.withA(0x60)},
                                 {65 * 65, color::Black.withA(0x15)},
                                 {71 * 71, color::Transparent},
                                 {77 * 77, color::Black.withA(0x15)},
                                 {92 * 92, color::Black.withA(0x60)}}));
  auto shadow =
      SmoothThickCircle({100 - 5, 100 + 5}, 71, 42, color::Black.withA(0x80));

  RasterizableStack stack(Box(0, 0, 200, 200));
  stack.addInput(&circle);
  stack.addInput(&rect1).withMode(BLENDING_MODE_SOURCE_ATOP);
  stack.addInput(&rect2).withMode(BLENDING_MODE_SOURCE_ATOP);
  stack.addInput(&gradient).withMode(BLENDING_MODE_SOURCE_ATOP);
  stack.addInput(&shadow).withMode(BLENDING_MODE_DESTINATION_OVER);
  {
    DrawingContext dc(display);
    dc.draw(stack, kCenter | kMiddle);
  }

  delay(2000);
}

class HeatingSpiralContent : public RasterizableStack {
 public:
  HeatingSpiralContent()
      : RasterizableStack(Box(0, 0, 99, 219)), thickness_(8) {
    for (int i = 0; i < 12; ++i) {
      elements_r_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 25},
                          thickness_, color::Black);
      addInput(&elements_r_[i]);
    }
    for (int i = 0; i < 12; ++i) {
      elements_l_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 39},
                          thickness_, color::Black, ENDING_FLAT);
      addInput(&elements_l_[i]);
    }
    outlet_ = SmoothThickLine({90, 25}, {100, 25}, thickness_, color::Black);
    addInput(&outlet_);
    inlet_ = SmoothThickLine({90, 12 * 14 + 25}, {100, 12 * 14 + 25},
                             thickness_, color::Black);
    addInput(&inlet_);
  }

 private:
  float thickness_;
  SmoothShape elements_l_[12];
  SmoothShape elements_r_[12];
  SmoothShape outlet_;
  SmoothShape inlet_;
};

class HeatingSpiralBody : public RasterizableStack {
 public:
  HeatingSpiralBody()
      : RasterizableStack(Box(0, 0, 99, 219)),
        thickness_outer_(10),
        thickness_inner_(7) {
    outlet_ =
        SmoothThickLine({90, 25}, {100, 25}, thickness_outer_, color::Black);
    addInput(&outlet_);
    for (int i = 0; i < 12; ++i) {
      elements_r_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 25},
                          thickness_outer_, color::Black);
      addInput(&elements_r_[i]);
    }
    inlet_ = SmoothThickLine({90, 12 * 14 + 25}, {100, 12 * 14 + 25},
                             thickness_outer_, color::Black);
    addInput(&inlet_);
    outlet_i_ =
        SmoothThickLine({90, 25}, {100, 25}, thickness_inner_, color::Black);
    addInput(&outlet_i_).withMode(BLENDING_MODE_DESTINATION_OUT);
    for (int i = 0; i < 12; ++i) {
      elements_ri_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 25},
                          thickness_inner_, color::White);
      addInput(&elements_ri_[i]).withMode(BLENDING_MODE_DESTINATION_OUT);
    }
    for (int i = 0; i < 12; ++i) {
      elements_l_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 39},
                          thickness_outer_, color::Black, ENDING_FLAT);
      addInput(&elements_l_[i]);
      elements_li_[i] =
          SmoothThickLine({10, (float)i * 14 + 32}, {90, (float)i * 14 + 39},
                          thickness_inner_, color::White);
      addInput(&elements_li_[i]).withMode(BLENDING_MODE_DESTINATION_OUT);
    }
    inlet_i_ = SmoothThickLine({90, 12 * 14 + 25}, {100, 12 * 14 + 25},
                               thickness_inner_, color::Black);
    addInput(&inlet_i_).withMode(BLENDING_MODE_DESTINATION_OUT);
  }

 private:
  float thickness_outer_;
  float thickness_inner_;
  SmoothShape elements_l_[12];
  SmoothShape elements_li_[12];
  SmoothShape elements_r_[12];
  SmoothShape elements_ri_[12];
  SmoothShape outlet_;
  SmoothShape outlet_i_;
  SmoothShape inlet_;
  SmoothShape inlet_i_;
};

class Tank : public RasterizableStack {
 public:
  Tank()
      : RasterizableStack(Box(0, 0, 99, 219)),
        casing_(Box(0, 0, 99, 219)),
        casing_body_(SmoothFilledRoundRect(0, 0, 99, 219, 23, color::Black)),
        casing_gradient_(
            {0, 0}, 0, 1,
            ColorGradient({{0, color::Purple}, {219, color::Blue}})),
        casing_wall_(SmoothThickRoundRect(0, 0, 99, 219, 23, 1, color::Black)),
        heated_spiral_(Box(0, 0, 99, 219)),
        heated_spiral_gradient_(
            {0, 0}, 0, 1, ColorGradient({{0, color::Blue}, {219, color::Red}})),
        heated_spiral_content_(),
        heating_spiral_body_() {
    casing_.addInput(&casing_body_);
    casing_.addInput(&casing_gradient_).withMode(BLENDING_MODE_SOURCE_ATOP);
    casing_.addInput(&casing_wall_);
    addInput(&casing_);
    heated_spiral_.addInput(&heated_spiral_content_);
    heated_spiral_.addInput(&heated_spiral_gradient_)
        .withMode(BLENDING_MODE_SOURCE_ATOP);
    addInput(&heated_spiral_);
    addInput(&heating_spiral_body_);
  }

 private:
  RasterizableStack casing_;
  SmoothShape casing_body_;
  LinearGradient casing_gradient_;
  SmoothShape casing_wall_;
  RasterizableStack heated_spiral_;
  LinearGradient heated_spiral_gradient_;
  HeatingSpiralContent heated_spiral_content_;
  HeatingSpiralBody heating_spiral_body_;
};

void rasterizable_stack_heating_tank() {
  display.setBackgroundColor(Graylevel(0xF0));
  display.clear();

  DrawingContext dc(display);
  std::unique_ptr<Tank> tank(new Tank());
  dc.draw(*tank, kCenter | kMiddle);

  delay(2000);
}

void loop() {
  rasterizable_stack_lifebuoy();
  rasterizable_stack_heating_tank();
}

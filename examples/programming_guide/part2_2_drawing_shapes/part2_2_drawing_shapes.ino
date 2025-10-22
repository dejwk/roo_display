// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#drawing-shapes

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
#include "roo_display/driver/ili9341.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Bold/15.h"

void setup() {
  SPI.begin(kSpiSck, kSpiMiso, kSpiMosi);
  display.init(Graylevel(0xF0));

  // Uncomment if using backlit.
  // backlit.begin();
}

void basic() {
  display.clear();

  DrawingContext dc(display);

  dc.draw(FilledCircle::ByExtents(10, 10, 80, color::Purple));
  dc.draw(Circle::ByExtents(10, 110, 80, color::Purple));
  dc.draw(FilledRect(110, 10, 159, 89, color::Red));
  dc.draw(Rect(110, 110, 159, 189, color::Red));
  dc.draw(FilledRoundRect(180, 10, 229, 89, 8, color::Blue));
  dc.draw(RoundRect(180, 110, 229, 189, 8, color::Blue));
  dc.draw(FilledTriangle({245, 10}, {305, 20}, {250, 89}, color::Orange));
  dc.draw(Triangle({245, 110}, {305, 120}, {250, 189}, color::Orange));

  dc.draw(Line({10, 210}, {305, 230}, color::Brown));
  dc.draw(Line({10, 230}, {305, 210}, color::Brown));

  delay(2000);
}

void antialiased() {
  display.clear();

  DrawingContext dc(display);

  dc.draw(SmoothFilledCircle({45.5, 45.5}, 40, color::Purple));
  dc.draw(SmoothCircle({45.5, 145.5}, 39.5, color::Purple));
  dc.draw(SmoothFilledRoundRect(179.5, 9.5, 229.5, 89.5, 7.5, color::Blue));
  dc.draw(SmoothRoundRect(180, 110, 229, 189, 8, color::Blue));
  dc.draw(SmoothFilledTriangle({245, 10}, {305, 20}, {250, 89}, color::Orange));

  dc.draw(SmoothLine({10, 210}, {305, 230}, color::Brown));
  dc.draw(SmoothLine({10, 230}, {305, 210}, color::Brown));

  delay(2000);
}

void small_circles() {
  display.clear();

  DrawingContext dc(display);
  dc.setTransformation(Transformation().scale(5, 5));

  dc.draw(SmoothFilledCircle({3, 5}, 0.5, color::Black));
  dc.draw(SmoothFilledCircle({10, 5}, 1, color::Black));
  dc.draw(SmoothFilledCircle({17, 5}, 1.5, color::Black));
  dc.draw(SmoothFilledCircle({25, 5}, 2, color::Black));
  dc.draw(SmoothFilledCircle({35, 5}, 2.5, color::Black));
  dc.draw(SmoothFilledCircle({45, 5}, 3, color::Black));
  dc.draw(SmoothFilledCircle({55, 5}, 3.5, color::Black));

  dc.draw(SmoothCircle({3, 15}, 0.5, color::Black));
  dc.draw(SmoothCircle({10, 15}, 1, color::Black));
  dc.draw(SmoothCircle({17, 15}, 1.5, color::Black));
  dc.draw(SmoothCircle({25, 15}, 2, color::Black));
  dc.draw(SmoothCircle({35, 15}, 2.5, color::Black));
  dc.draw(SmoothCircle({45, 15}, 3, color::Black));
  dc.draw(SmoothCircle({55, 15}, 3.5, color::Black));

  dc.draw(SmoothFilledCircle({3.5, 25.5}, 0.5, color::Black));
  dc.draw(SmoothFilledCircle({10.5, 25.5}, 1, color::Black));
  dc.draw(SmoothFilledCircle({17.5, 25.5}, 1.5, color::Black));
  dc.draw(SmoothFilledCircle({25.5, 25.5}, 2, color::Black));
  dc.draw(SmoothFilledCircle({35.5, 25.5}, 2.5, color::Black));
  dc.draw(SmoothFilledCircle({45.5, 25.5}, 3, color::Black));
  dc.draw(SmoothFilledCircle({55.5, 25.5}, 3.5, color::Black));

  dc.draw(SmoothCircle({3.5, 35.5}, 0.5, color::Black));
  dc.draw(SmoothCircle({10.5, 35.5}, 1, color::Black));
  dc.draw(SmoothCircle({17.5, 35.5}, 1.5, color::Black));
  dc.draw(SmoothCircle({25.5, 35.5}, 2, color::Black));
  dc.draw(SmoothCircle({35.5, 35.5}, 2.5, color::Black));
  dc.draw(SmoothCircle({45.5, 35.5}, 3, color::Black));
  dc.draw(SmoothCircle({55.5, 35.5}, 3.5, color::Black));

  delay(2000);
}

void circle_thickness() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 10; ++i) {
    float dx = 30 * i;
    dc.draw(SmoothThickCircle({25 + dx, 25}, 10, 0.25 * (i + 1), color::Black));
    dc.draw(
        SmoothThickCircle({25 + dx, 55}, 10, 0.25 * (i + 11), color::Black));
    dc.draw(
        SmoothThickCircle({25 + dx, 85}, 10, 0.25 * (i + 21), color::Black));
    dc.draw(
        SmoothThickCircle({25 + dx, 115}, 10, 0.25 * (i + 31), color::Black));

    dc.draw(SmoothThickRoundRect(15 + dx, 150, 35 + dx, 170, 5, 0.25 * (i + 1),
                                 color::Black));
    dc.draw(SmoothThickRoundRect(15 + dx, 180, 35 + dx, 200, 5, 0.25 * (i + 11),
                                 color::Black));
    dc.draw(SmoothThickRoundRect(15 + dx, 210, 35 + dx, 230, 5, 0.25 * (i + 21),
                                 color::Black));
  }

  delay(2000);
}

void interiors() {
  display.clear();

  DrawingContext dc(display);
  dc.draw(SmoothThickCircle({80, 100}, 50, 10, color::DarkSlateBlue,
                            color::Orange));

  Box btn(190, 80, 279, 119);
  dc.draw(SmoothThickRoundRect(btn.xMin(), btn.yMin(), btn.xMax(), btn.yMax(),
                               8, 3.5, color::Navy, color::LightGray));
  dc.setBackgroundColor(color::LightGray);
  dc.draw(MakeTileOf(TextLabel("OK", font_NotoSans_Bold_15(), color::Black),
                     btn, kCenter | kMiddle, color::Transparent));

  delay(2000);
}

void line_thickness() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 50; ++i) {
    float dy = 10 * i - 15;
    dc.draw(SmoothThickLine({25, dy}, {294, dy + 20}, 0.25 * (i + 1),
                            color::Black));
  }

  delay(2000);
}

void thick_round_endings() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 6; ++i) {
    float dy = 25 * i + 2.5 * i * i + 10;
    dc.draw(
        SmoothThickLine({25, dy}, {294, dy + 20}, 5 * (i + 1), color::Black));
  }

  delay(2000);
}

void thick_flat_endings() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 6; ++i) {
    float dy = 25 * i + 2.5 * i * i + 10;
    dc.draw(SmoothThickLine({25, dy}, {294, dy + 20}, 5 * (i + 1), color::Black,
                            ENDING_FLAT));
  }

  delay(2000);
}

void rotated_rectangles() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 12; ++i) {
    float x = cosf(2 * M_PI * i / 12);
    float y = sinf(2 * M_PI * i / 12);
    dc.draw(SmoothThickLine({160 + x * 70, 120 + y * 70},
                            {160 + x * 100, 120 + y * 100}, 20, color::Black,
                            ENDING_FLAT));
  }

  dc.draw(
      SmoothRotatedFilledRect({160, 120}, 50, 50, M_PI / 4, color::Crimson));

  delay(2000);
}

void wedges() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 12; ++i) {
    float x = cosf(2 * M_PI * i / 12);
    float y = sinf(2 * M_PI * i / 12);
    dc.draw(SmoothWedgedLine({160 + x * 90, 120 + y * 90}, 40,
                             {160 + x * 115, 120 + y * 115}, 50, color::Black,
                             ENDING_FLAT));

    dc.draw(SmoothWedgedLine({160 + x * 50, 120 + y * 50}, 20,
                             {160 + x * 80, 120 + y * 80}, 8, color::Blue,
                             ENDING_ROUNDED));
  }
  dc.draw(
      SmoothRotatedFilledRect({160, 120}, 50, 50, M_PI / 4, color::Crimson));

  delay(2000);
}

void more_wedges() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 15; ++i) {
    dc.draw(SmoothWedgedLine({20 + i * 20, 120}, 15, {20 + i * 20, 10}, 14 - i,
                             color::Black));
  }

  for (int i = 0; i < 7; ++i) {
    dc.draw(SmoothWedgedLine({20 + i * 45, 210}, 4 + i * 5, {20 + i * 45, 140},
                             0, color::DarkRed));
  }

  delay(2000);
}

void arcs() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 10; ++i) {
    dc.draw(SmoothArc({160, 120}, i * 10, (i + 1) * -0.3, (i + 1) * 0.3,
                      color::Black));
  }

  delay(2000);
}

void thick_arcs() {
  display.clear();

  DrawingContext dc(display);
  for (int i = 0; i < 6; ++i) {
    dc.draw(SmoothThickArc({160, 120}, i * 21, 18, (i + 1) * -0.5,
                           (i + 1) * 0.5, HsvToRgb(i * 15, 1.0f, 0.7f),
                           ENDING_ROUNDED));
  }

  delay(2000);
}

void gauge() {
  display.clear();

  DrawingContext dc(display);
  float radius = 120;
  float width = 30;
  FpPoint center{130, 190};
  dc.draw(SmoothThickArc(center, radius, width, -M_PI * 1 / 6 - 0.05,
                         M_PI * 1 / 2 + 0.05, color::LightGray, ENDING_FLAT));
  dc.setBackgroundColor(color::LightGray);
  for (int i = 0; i < 51; ++i) {
    float angle = (i / 50.0f * 2 / 3 - 1.0f / 6.0f) * M_PI;
    Serial.println(angle);
    float x = sinf(angle);
    float y = -cosf(angle);
    float tick_inner_radius = radius - width / 2 + 5;
    float tick_len = (i % 5 == 0 ? 20 : 10);
    float tick_radius = tick_inner_radius + tick_len;
    dc.draw(SmoothLine({x * tick_inner_radius, y * tick_inner_radius},
                       {x * tick_radius, y * tick_radius}, color::Black),
            center.x, center.y);
  }
  float needle_radius = radius - width / 2 - 5;
  float needle_angle = 0.1 * M_PI;
  float needle_x = sinf(needle_angle);
  float needle_y = -cosf(needle_angle);
  dc.draw(SmoothWedgedLine({center}, 10,
                           {center.x + needle_x * needle_radius,
                            center.y + needle_y * needle_radius},
                           0, color::Red));

  delay(2000);
}

void pies() {
  display.clear();

  DrawingContext dc(display);
  float radius = 90;
  FpPoint center{160, 120};
  float areas[] = {0.35, 0.27, 0.2, 0.18};
  Color colors[] = {color::Red, color::Green, color::Orange, color::RoyalBlue};
  float start_angle = 0.0f;
  for (int i = 0; i < 4; ++i) {
    float angle = areas[i] * (2 * M_PI);
    float end_angle = start_angle + angle;
    float mid_angle = start_angle + angle * 0.5f;
    dc.draw(SmoothPie(
        {center.x + sinf(mid_angle) * 1, center.y - cosf(mid_angle) * 1},
        radius, start_angle, end_angle, colors[i]));
    start_angle += angle;
  }

  delay(2000);
}

void loop() {
  basic();
  antialiased();
  small_circles();
  circle_thickness();
  interiors();
  line_thickness();
  thick_round_endings();
  thick_flat_endings();
  rotated_rectangles();
  wedges();
  more_wedges();
  arcs();
  thick_arcs();
  gauge();
  pies();
}

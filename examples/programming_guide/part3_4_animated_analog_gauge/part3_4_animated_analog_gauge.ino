// https://github.com/dejwk/roo_display/blob/master/doc/programming_guide.md#advanced-example-animated-analog-gauge
//
// https://www.youtube.com/watch?v=kkTuxA0qa34

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

#include "roo_display/filter/foreground.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/point.h"
#include "roo_display/shape/smooth.h"
#include "roo_display/ui/string_printer.h"
#include "roo_display/ui/text_label.h"
#include "roo_display/ui/tile.h"
#include "roo_smooth_fonts/NotoSans_Bold/27.h"

void setup() {
  SPI.begin();
  display.init(color::White);
}

static const float kRadius = 120;
static const float kWidth = 30;
static const FpPoint kGaugeCenter{160, 190};
static const Box kTextExtents(kGaugeCenter.x - 45,
                              kGaugeCenter.y - kRadius / 2 - 20,
                              kGaugeCenter.x + 44,
                              kGaugeCenter.y - kRadius / 2);

// Helper class that generates the needle drawable, given the value it should
// indicate.
class Needle {
 public:
  // Value expected to swing between 0 and 1, inclusive.
  Needle(float value) : value_(value) {}

  SmoothShape getShape() {
    float needle_radius = kRadius - kWidth / 2 + 20;
    float needle_angle = (value_ * 2.0f / 3 - 1.0f / 3) * M_PI;
    float needle_x = sinf(needle_angle);
    float needle_y = -cosf(needle_angle);
    return SmoothWedgedLine({kGaugeCenter}, 10,
                            {kGaugeCenter.x + needle_x * needle_radius,
                             kGaugeCenter.y + needle_y * needle_radius},
                            0, color::Red);
  }

 private:
  float value_;
};

// Drawable that prints the numeric value in 'front' of the indicator, with the
// needle behind it.
class ValueIndicator : public Drawable {
 public:
  // Value expected to swing between 0 and 1, inclusive.
  ValueIndicator(float value) : value_(value) {}

  // The extents area covers the entire indicator, so that the needle (set as a
  // background) gets drawn fully, regardless of its position. To make sure we
  // don't overwrite the scale, it is drawn first (in the write-once mode).
  Box extents() const override {
    return Box(kGaugeCenter.x - kRadius - kWidth / 2 + 10,
               kGaugeCenter.y - kRadius - kWidth / 2 - 20 + 10,
               kGaugeCenter.x + kRadius + kWidth / 2 - 10,
               kGaugeCenter.y - 15 - 10);
  }

 private:
  void drawTo(const Surface &s) const override {
    Needle needle(value_);
    auto needle_shape = needle.getShape();
    DrawingContext dc(s);
    dc.setBackground(&needle_shape);
    dc.draw(MakeTileOf(TextLabel(StringPrintf("%0.1f%%", value_ * 100),
                                 font_NotoSans_Bold_27(), color::Black),
                       kTextExtents, kCenter | kMiddle));
    // We're expecting to be called in the write-once mode, when everything else
    // (e.g. the scale) is drawn. Calling clear() at the very end to make sure
    // that we get all of the background needle.
    dc.clear();
  }

  float value_;
};

// Drawable that prints the radial scale of our indicator, with the needle
// overlaid in front of it.
class Scale : public Drawable {
 public:
  Scale(float value) : value_(value) {}

  Box extents() const override {
    return Box(kGaugeCenter.x - kRadius - kWidth / 2 + 10,
               kGaugeCenter.y - kRadius - kWidth / 2 - 20 + 10,
               kGaugeCenter.x + kRadius + kWidth / 2 - 10,
               kGaugeCenter.y - 15 - 10);
  }

 private:
  void drawTo(const Surface &s) const override {
    Needle needle(value_);
    auto needle_shape = needle.getShape();
    Surface my_s = s;
    ForegroundFilter filter(s.out(), &needle_shape);
    my_s.set_out(&filter);
    DrawingContext dc(my_s);
    // Note: we expect my_s to be write-once.
    dc.setBackgroundColor(color::LightGray);
    for (int i = 0; i < 51; ++i) {
      float angle = (i / 50.0f * 2 / 3 - 1.0f / 3.0f) * M_PI;
      float x = sinf(angle);
      float y = -cosf(angle);
      float tick_inner_radius = kRadius - kWidth / 2 + 5;
      float tick_len = (i % 5 == 0 ? 20 : 10);
      float tick_radius = tick_inner_radius + tick_len;
      dc.draw(SmoothLine({x * tick_inner_radius, y * tick_inner_radius},
                         {x * tick_radius, y * tick_radius}, color::Black),
              kGaugeCenter.x, kGaugeCenter.y);
    }
    dc.setBackgroundColor(color::White);
    dc.draw(SmoothThickArc(kGaugeCenter, kRadius, kWidth, -M_PI * 1 / 3 - 0.05,
                           M_PI * 1 / 3 + 0.05, color::LightGray, ENDING_FLAT));
  }

  float value_;
};

// Puts it all together.
class Gauge : public Drawable {
 public:
  Gauge() : value_(0), full_redraw_(true) {}

  void setValue(float value) {
    previous_value_ = value_;
    value_ = value;
  }

  // If true, redraws the entire content. Otherwise, assumes that setValue()
  // has been called to update the value, and draws the minimum amount of pixels
  // for an incremental update.
  void setFullRedraw(bool full_redraw) { full_redraw_ = full_redraw; }

  Box extents() const override {
    return Box(kGaugeCenter.x - kRadius - kWidth / 2,
               kGaugeCenter.y - kRadius - kWidth / 2 - 20,
               kGaugeCenter.x + kRadius + kWidth / 2, kGaugeCenter.y - 15);
  }

 private:
  void drawTo(const Surface &s) const override {
    Box box = extents();
    if (full_redraw_) {
      s.drawObject(SmoothThickRoundRect(box.xMin() + 3, box.yMin() + 3,
                                        box.xMax() - 3, box.yMax() - 3, 8, 6,
                                        color::Orange));
    }
    DrawingContext dc(s);
    dc.setWriteOnce();
    if (full_redraw_) {
      // Simple case: we just draw the scale and the value indicator. Due to
      // write-once, value indicator will not overwrite the scale.
      dc.draw(Scale(value_));
      dc.draw(ValueIndicator(value_));
    } else {
      // Incremental case is more interesting. We need to redraw all pixels that
      // are affected by the union of the old and the new needle position, and
      // we need to update the text value.
      Needle old_needle(previous_value_);
      Needle new_needle(value_);
      auto old_needle_shape = old_needle.getShape();
      auto new_needle_shape = new_needle.getShape();
      Box extents = Box::Extent(
          Box::Intersect(dc.getClipBox(), old_needle_shape.extents()),
          Box::Intersect(dc.getClipBox(), new_needle_shape.extents()));
      extents = Box::Extent(extents, kTextExtents);
      // Using bitmask to narrow down to just the pixels that absolutely need to
      // be updated.
      BitMaskOffscreen bitmask(extents);
      DrawingContext bitmask_dc(bitmask);
      bitmask_dc.clear();
      // We need to redraw the text. (For simplicity, we're assuming that the
      // entire text tile needs to be invalidated).
      bitmask_dc.draw(FilledRect(kTextExtents, color::Black));
      // We also need to redraw the pixels affected by the old and the new
      // needle. They can be determined simply by drawing the needles to the
      // bitmask offscreen.
      bitmask_dc.draw(old_needle_shape);
      bitmask_dc.draw(new_needle_shape);

      // Now, let's apply the clip mask and the clip box to our context, and
      // just draw the content as before. Note that we're inverting the mask,
      // because the '1's now indicate pixels that *should* be drawn.
      dc.setClipBox(extents);
      ClipMask mask(bitmask.buffer(), extents);
      dc.setClipMask(&mask);
      mask.setInverted(true);
      dc.draw(Scale(value_));
      dc.draw(ValueIndicator(value_));
    }
  }

  float previous_value_;
  float value_;
  bool full_redraw_;
};

Gauge gauge;

void loop() {
  gauge.setValue((sinf(millis() / 5000.0) + 1) / 2);
  DrawingContext dc(display);
  dc.draw(gauge);
  gauge.setFullRedraw(false);
  delay(20);
}

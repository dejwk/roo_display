#include "roo_display.h"

#include <Arduino.h>

#include <cmath>

#include "roo_display/filter/color_filter.h"

static const int smoothingWindowMs = 10;
static const float smoothingFactor = 0.8;  // Closer to 1 -> more smoothing.

namespace roo_display {

bool TouchDisplay::getTouch(int16_t& x, int16_t& y) {
  int16_t raw_x, raw_y, raw_z;
  bool touched = touch_device_.getTouch(raw_x, raw_y, raw_z);
  if (!touched) {
    touched_ = false;
    return false;
  }
  long now = millis();
  Orientation orientation = display_device_.orientation();
  if (orientation.isRightToLeft()) {
    raw_x = 4095 - raw_x;
  }
  if (orientation.isBottomToTop()) {
    raw_y = 4095 - raw_y;
  }
  if (orientation.isXYswapped()) {
    std::swap(raw_x, raw_y);
  }
  if (!touched_) {
    raw_touch_x_ = raw_x;
    raw_touch_y_ = raw_y;
    raw_touch_z_ = raw_z;
    touched_ = true;
  } else if (now != last_sample_time_) {
    float k = (float)(now - last_sample_time_) / smoothingWindowMs;
    float weight_past = pow(smoothingFactor, k);
    float weight_present = 1 - weight_past;
    raw_touch_x_ =
        (int16_t)(raw_touch_x_ * weight_past + raw_x * weight_present);
    raw_touch_y_ =
        (int16_t)(raw_touch_y_ * weight_past + raw_y * weight_present);
    raw_touch_z_ =
        (int16_t)(raw_touch_z_ * weight_past + raw_z * weight_present);
    last_sample_time_ = now;
  }

  x = ((int32_t)raw_touch_x_ * (display_device_.effective_width() - 1)) / 4095;
  y = ((int32_t)raw_touch_y_ * (display_device_.effective_height() - 1)) / 4095;
  return true;
}

class DummyTouch : public TouchDevice {
 public:
  bool getTouch(int16_t& x, int16_t& y, int16_t& z) override { return false; }
};

static DummyTouch dummy_touch;

Display::Display(DisplayDevice& display_device, TouchDevice* touch_device)
    : display_device_(display_device),
      touch_(display_device,
             touch_device == nullptr ? dummy_touch : *touch_device),
      nest_level_(0),
      orientation_(display_device.orientation()),
      extents_(Box::MaximumBox()),
      bgcolor_(Color(0)),
      background_(nullptr) {
  resetExtents();
}

void Display::setOrientation(Orientation orientation) {
  if (orientation_ == orientation) return;
  orientation_ = orientation;
  nest();
  display_device_.setOrientation(orientation);
  unnest();
  // Update, since the width and height might have gotten swapped.
  resetExtents();
}

void Display::init(Color bgcolor) {
  display_device_.init();
  nest();
  display_device_.fillRect(Box(0, 0, display_device_.effective_width() - 1,
                               display_device_.effective_height() - 1),
                           bgcolor);
  unnest();
  setBackground(bgcolor);
  display_device_.setBgColorHint(bgcolor);
}

void Display::clear() {
  DrawingContext dc(*this);
  dc.clear();
}

void DrawingContext::fill(Color color) { draw(Fill(color)); }
void DrawingContext::clear() { draw(Clear()); }

namespace {

class Pixels : public Drawable {
 public:
  Pixels(const std::function<void(ClippingBufferedPixelWriter&)>& fn,
         Box extents, PaintMode paint_mode)
      : fn_(fn), extents_(extents), paint_mode_(paint_mode) {}

  Box extents() const override { return extents_; }

 private:
  void drawTo(const Surface& s) const override {
    if (s.dx() == 0 && s.dy() == 0) {
      ClippingBufferedPixelWriter writer(s.out(), extents_, paint_mode_);
      fn_(writer);
    } else {
      TransformedDisplayOutput out(s.out(),
                                   Transform().translate(s.dx(), s.dy()));
      ClippingBufferedPixelWriter writer(out, extents_, paint_mode_);
      fn_(writer);
    }
  }

  const std::function<void(ClippingBufferedPixelWriter&)>& fn_;
  Box extents_;
  PaintMode paint_mode_;
};

}  // namespace

DrawingContext::~DrawingContext() { unnest_(); }

void DrawingContext::setWriteOnce() {
  if (write_once_) return;
  write_once_ = true;
  front_to_back_writer_.reset(
      new FrontToBackWriter(output(), bounds().translate(dx_, dy_)));
}

void DrawingContext::drawPixels(
    const std::function<void(ClippingBufferedPixelWriter&)>& fn,
    PaintMode paint_mode) {
  draw(Pixels(fn, transform_.smallestEnclosingRect(clip_box_), paint_mode));
}

void DrawingContext::drawInternal(const Drawable& object, int16_t dx,
                                  int16_t dy, Color bgcolor) {
  DisplayOutput& out = front_to_back_writer_.get() != nullptr
                           ? *front_to_back_writer_
                           : output();
  if (clip_mask_ == nullptr) {
    drawInternalWithBackground(out, object, dx, dy, bgcolor);
  } else {
    ClipMaskFilter filter(out, clip_mask_);
    drawInternalWithBackground(filter, object, dx, dy, bgcolor);
  }
}

void DrawingContext::drawInternalWithBackground(DisplayOutput& output,
                                                const Drawable& object,
                                                int16_t dx, int16_t dy,
                                                Color bgcolor) {
  if (background_ == nullptr) {
    drawInternalTransformed(output, object, dx, dy, bgcolor);
  } else {
    BackgroundFilter filter(output, background_);
    drawInternalTransformed(filter, object, dx, dy, bgcolor);
  }
}

void DrawingContext::drawInternalTransformed(DisplayOutput& output,
                                             const Drawable& object, int16_t dx,
                                             int16_t dy, Color bgcolor) {
  Surface s(output, dx + dx_, dy + dy_, clip_box_.translate(dx_, dy_),
            write_once_, bgcolor, fill_mode_, paint_mode_);
  if (!transformed_) {
    s.drawObject(object);
  } else if (!transform_.is_rescaled() && !transform_.xy_swap()) {
    // Translation only.
    s.set_dx(s.dx() + transform_.x_offset());
    s.set_dy(s.dy() + transform_.y_offset());
    s.drawObject(object);
  } else {
    s.drawObject(TransformedDrawable(transform_, &object));
  }
}

namespace {

class ErasedDrawable : public Drawable {
 public:
  ErasedDrawable(const Drawable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }
  Box anchorExtents() const override { return delegate_->anchorExtents(); }

 private:
  void drawTo(const Surface& s) const override {
    Surface news = s;
    ColorFilter<Erasure> filter(s.out());
    news.set_out(&filter);
    news.set_paint_mode(PAINT_MODE_REPLACE);
    news.drawObject(*delegate_);
  }

  const Drawable* delegate_;
};

}  // namespace

void DrawingContext::erase(const Drawable& object) {
  draw(ErasedDrawable(&object));
}

void DrawingContext::erase(const Drawable& object, int16_t dx, int16_t dy) {
  draw(ErasedDrawable(&object), dx, dy);
}

void DrawingContext::erase(const Drawable& object, int16_t dx, int16_t dy,
                           Alignment alignment) {
  draw(ErasedDrawable(&object), dx, dy, alignment);
}

}  // namespace roo_display

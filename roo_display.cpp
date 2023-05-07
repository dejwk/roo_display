#include "roo_display.h"

#include <Arduino.h>

#include <cmath>

#include "roo_display/filter/color_filter.h"

namespace roo_display {

TouchResult TouchDisplay::getTouch(TouchPoint* points, int max_points) {
  TouchResult result = touch_device_.getTouch(points, max_points);
  if (result.touch_points > 0) {
    Orientation orientation = display_device_.orientation();
    for (int i = 0; i < result.touch_points && i < max_points; ++i) {
      TouchPoint& p = points[i];
      touch_calibration_.Calibrate(p);
      if (orientation.isRightToLeft()) {
        p.x = 4095 - p.x;
        p.vx = -p.vx;
      }
      if (orientation.isBottomToTop()) {
        p.y = 4095 - p.y;
        p.vy = -p.vy;
      }
      if (orientation.isXYswapped()) {
        std::swap(p.x, p.y);
        std::swap(p.vx, p.vy);
      }
      p.x = ((int32_t)p.x * (display_device_.effective_width() - 1)) / 4095;
      p.y = ((int32_t)p.y * (display_device_.effective_height() - 1)) / 4095;
      p.vx = ((int32_t)p.vx * (display_device_.effective_width() - 1)) / 4095;
      p.vy = ((int32_t)p.vy * (display_device_.effective_height() - 1)) / 4095;
    }
  }
  return result;
}

bool Display::getTouch(int16_t& x, int16_t& y) {
  TouchPoint point;
  TouchResult result = touch_.getTouch(&point, 1);
  if (result.touch_points == 0) {
    return false;
  }
  x = point.x;
  y = point.y;
  return true;
}

class DummyTouch : public TouchDevice {
 public:
  void initTouch() override {}

  TouchResult getTouch(TouchPoint* points, int max_points) override {
    return TouchResult(micros(), 0);
  }
};

static DummyTouch dummy_touch;

Display::Display(DisplayDevice& display_device, TouchDevice* touch_device,
                 TouchCalibration touch_calibration)
    : display_device_(display_device),
      touch_(display_device,
             touch_device == nullptr ? dummy_touch : *touch_device,
             touch_calibration),
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
  init();
  setBackgroundColor(bgcolor);
  clear();
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
         Box extents, BlendingMode blending_mode)
      : fn_(fn), extents_(extents), blending_mode_(blending_mode) {}

  Box extents() const override { return extents_; }

 private:
  void drawTo(const Surface& s) const override {
    if (s.dx() == 0 && s.dy() == 0) {
      ClippingBufferedPixelWriter writer(s.out(), extents_, blending_mode_);
      fn_(writer);
    } else {
      TransformedDisplayOutput out(s.out(),
                                   Transformation().translate(s.dx(), s.dy()));
      ClippingBufferedPixelWriter writer(out, extents_, blending_mode_);
      fn_(writer);
    }
  }

  const std::function<void(ClippingBufferedPixelWriter&)>& fn_;
  Box extents_;
  BlendingMode blending_mode_;
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
    BlendingMode blending_mode) {
  draw(
      Pixels(fn, transformation_.smallestEnclosingRect(clip_box_), blending_mode));
}

void DrawingContext::drawInternal(const Drawable& object, int16_t dx,
                                  int16_t dy, Color bgcolor) {
  DisplayOutput& out = front_to_back_writer_.get() != nullptr
                           ? *front_to_back_writer_
                           : output();
  Surface s(out, dx + dx_, dy + dy_, clip_box_.translate(dx_, dy_), write_once_,
            bgcolor, fill_mode_, blending_mode_);
  if (clip_mask_ == nullptr) {
    drawInternalWithBackground(s, object);
  } else {
    ClipMaskFilter filter(out, clip_mask_);
    s.set_out(&filter);
    drawInternalWithBackground(s, object);
  }
}

void DrawingContext::drawInternalWithBackground(Surface& s,
                                                const Drawable& object) {
  if (background_ == nullptr) {
    drawInternalTransformed(s, object);
  } else if (s.bgcolor().a() == 0) {
    BackgroundFilter filter(s.out(), background_);
    s.set_out(&filter);
    drawInternalTransformed(s, object);
  } else {
    // Also apply the background color.
    BackgroundFilterWithColor filter(
        s.out(), internal::BgBlendKernelWithColor{.bg = s.bgcolor()},
        background_);
    s.set_out(&filter);
    s.set_bgcolor(color::Transparent);
    drawInternalTransformed(s, object);
  }
}

void DrawingContext::drawInternalTransformed(Surface& s,
                                             const Drawable& object) {
  if (!transformed_) {
    if (s.clipToExtents(object.extents()) == Box::CLIP_RESULT_EMPTY) return;
    s.drawObject(object);
  } else if (!transformation_.is_rescaled() && !transformation_.xy_swap()) {
    // Translation only.
    s.set_dx(s.dx() + transformation_.x_offset());
    s.set_dy(s.dy() + transformation_.y_offset());
    if (s.clipToExtents(object.extents()) == Box::CLIP_RESULT_EMPTY) return;
    s.drawObject(object);
  } else {
    auto transformed = TransformedDrawable(transformation_, &object);
    if (s.clipToExtents(transformed.extents()) == Box::CLIP_RESULT_EMPTY) {
      return;
    }
    s.drawObject(transformed);
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
    news.set_blending_mode(BLENDING_MODE_SOURCE);
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

void DrawingContext::erase(const Drawable& object, Alignment alignment) {
  draw(ErasedDrawable(&object), alignment);
}

}  // namespace roo_display

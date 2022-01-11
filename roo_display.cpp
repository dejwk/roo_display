#include "roo_display.h"

#include "roo_display/filter/color_filter.h"

namespace roo_display {

bool TouchDisplay::getTouch(int16_t& x, int16_t& y) {
  int16_t raw_x, raw_y, raw_z;
  bool touched = touch_.getTouch(raw_x, raw_y, raw_z);
  if (!touched) {
    return false;
  }
  Orientation orientation = display_.orientation();
  if (orientation.isRightToLeft()) {
    raw_x = 4096 - raw_x;
  }
  if (orientation.isBottomToTop()) {
    raw_y = 4096 - raw_y;
  }
  if (orientation.isXYswapped()) {
    std::swap(raw_x, raw_y);
  }

  x = ((int32_t)raw_x * display_.effective_width()) / 4096;
  y = ((int32_t)raw_y * display_.effective_height()) / 4096;
  return true;
}

class DummyTouch : public TouchDevice {
 public:
  bool getTouch(int16_t& x, int16_t& y, int16_t& z) override { return false; }
};

static DummyTouch dummy_touch;

Display::Display(DisplayDevice& display, TouchDevice* touch)
    : display_(display),
      touch_(display, touch == nullptr ? dummy_touch : *touch),
      nest_level_(0),
      orientation_(display.orientation()),
      clip_box_(Box::MaximumBox()),
      bgcolor_(Color(0)),
      background_(nullptr) {
  updateBounds();
}

void Display::setOrientation(Orientation orientation) {
  if (orientation_ == orientation) return;
  orientation_ = orientation;
  nest();
  display_.setOrientation(orientation);
  unnest();
  updateBounds();
}

void Display::init(Color bgcolor) {
  display_.init();
  nest();
  display_.fillRect(Box(0, 0, display_.effective_width() - 1,
                        display_.effective_height() - 1),
                    bgcolor);
  unnest();
  setBackground(bgcolor);
  display_.setBgColorHint(bgcolor);
}

void Display::clear() {
  DrawingContext dc(*this);
  dc.clear();
}

void Display::updateBounds() {
  if (orientation_.isXYswapped()) {
    width_ = display_.raw_height();
    height_ = display_.raw_width();
  } else {
    width_ = display_.raw_width();
    height_ = display_.raw_height();
  }
  clip_box_ = Box(0, 0, width_ - 1, height_ - 1);
}

void DrawingContext::fill(Color color) { draw(Fill(color)); }
void DrawingContext::clear() { draw(Clear()); }

namespace {

class ErasedDrawable : public Drawable {
 public:
  ErasedDrawable(const Drawable *delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

 private:
  void drawTo(const Surface &s) const override {
    Surface news = s;
    ColorFilter<Erasure> filter(s.out());
    news.set_out(&filter);
    news.set_paint_mode(PAINT_MODE_REPLACE);
    news.drawObject(*delegate_);
  }

  const Drawable *delegate_;
};

}  // namespace

void DrawingContext::erase(const Drawable &object) {
  draw(ErasedDrawable(&object));
}

void DrawingContext::erase(const Drawable &object, int16_t dx, int16_t dy) {
  draw(ErasedDrawable(&object), dx, dy);
}

void DrawingContext::erase(const Drawable &object, int16_t dx, int16_t dy,
                           HAlign halign, VAlign valign) {
  draw(ErasedDrawable(&object), dx, dy, halign, valign);
}

}  // namespace roo_display

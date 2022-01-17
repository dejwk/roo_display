#include "roo_display.h"

#include "roo_display/filter/color_filter.h"

namespace roo_display {

bool TouchDisplay::getTouch(int16_t& x, int16_t& y) {
  int16_t raw_x, raw_y, raw_z;
  bool touched = touch_device_.getTouch(raw_x, raw_y, raw_z);
  if (!touched) {
    return false;
  }
  Orientation orientation = display_device_.orientation();
  if (orientation.isRightToLeft()) {
    raw_x = 4096 - raw_x;
  }
  if (orientation.isBottomToTop()) {
    raw_y = 4096 - raw_y;
  }
  if (orientation.isXYswapped()) {
    std::swap(raw_x, raw_y);
  }

  x = ((int32_t)raw_x * display_device_.effective_width()) / 4096;
  y = ((int32_t)raw_y * display_device_.effective_height()) / 4096;
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

class ErasedDrawable : public Drawable {
 public:
  ErasedDrawable(const Drawable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

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
                           HAlign halign, VAlign valign) {
  draw(ErasedDrawable(&object), dx, dy, halign, valign);
}

}  // namespace roo_display

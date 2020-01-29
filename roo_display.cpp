#include "roo_display.h"

namespace roo_display {

bool TouchDisplay::getTouch(int16_t *x, int16_t *y) {
  int16_t raw_x, raw_y, raw_z;
  bool touched = touch_->getTouch(&raw_x, &raw_y, &raw_z);
  if (!touched) {
    return false;
  }
  Orientation orientation = display_->orientation();
  if (orientation.isXYswapped()) {
    std::swap(raw_x, raw_y);
  }
  if (orientation.isRightToLeft()) {
    raw_x = 4096 - raw_x;
  }
  if (orientation.isBottomToTop()) {
    raw_y = 4096 - raw_y;
  }

  *x = ((int32_t)raw_x * display_->effective_width()) / 4096;
  *y = ((int32_t)raw_y * display_->effective_height()) / 4096;
  return true;
}

class DummyTouch : public TouchDevice {
 public:
  bool getTouch(int16_t *x, int16_t *y, int16_t *z) override { return false; }
};

static DummyTouch dummy_touch;

Display::Display(DisplayDevice *display, TouchDevice *touch)
    : display_(display),
      touch_(display, touch == nullptr ? &dummy_touch : touch),
      orientation_(display->orientation()) {
  updateBounds();
}

void Display::setOrientation(Orientation orientation) {
  if (orientation_ == orientation) return;
  orientation_ = orientation;
  nest();
  display_->setOrientation(orientation);
  unnest();
  updateBounds();
}

void Display::init(Color bgcolor) {
  display_->init();
  nest();
  display_->fillRect(Box(0, 0, display_->effective_width() - 1,
                         display_->effective_height() - 1),
                     bgcolor);
  unnest();
  display_->setBgColorHint(bgcolor);
}

void Display::updateBounds() {
  if (orientation_.isXYswapped()) {
    width_ = display_->raw_height();
    height_ = display_->raw_width();
  } else {
    width_ = display_->raw_width();
    height_ = display_->raw_height();
  }
}

void DrawingContext::fill(Color color) { draw(Fill(color)); }

}  // namespace roo_display

// Support for drawing to in-memory buffers, using various color modes.

#include "roo_display/core/byte_order.h"
#include "roo_display/core/color.h"
#include "roo_display/core/memfill.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/core/raster.h"

namespace roo_display {
namespace internal {
    
AddressWindow::AddressWindow()
    : orientation_(Orientation::Default()),
      offset_(0),
      x0_(0),
      x1_(-1),
      y0_(0),
      y1_(-1),
      advance_x_(0),
      advance_y_(0),
      cursor_x_(0),
      cursor_y_(0) {}

void AddressWindow::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                               uint16_t y1, int16_t raw_width,
                               int16_t raw_height, Orientation orientation) {
  orientation_ = orientation;
  x0_ = x0;
  y0_ = y0;
  x1_ = x1;
  y1_ = y1;
  cursor_x_ = x0_;
  cursor_y_ = y0_;
  int8_t x_direction = orientation.isLeftToRight() ? 1 : -1;
  int8_t y_direction = orientation.isTopToBottom() ? 1 : -1;

  if (orientation.isXYswapped()) {
    offset_ = (x_direction > 0 ? y0_ : raw_width - 1 - y0_) +
              (y_direction > 0 ? x0_ : raw_height - 1 - x0_) * raw_width;
    advance_x_ = y_direction * raw_width;
    advance_y_ = x_direction - y_direction * (x1_ - x0_ + 1) * raw_width;
  } else {
    offset_ = (x_direction > 0 ? x0_ : raw_width - 1 - x0_) +
              (y_direction > 0 ? y0_ : raw_height - 1 - y0_) * raw_width;
    advance_x_ = x_direction;
    advance_y_ = y_direction * raw_width - x_direction * (x1_ - x0_ + 1);
  }
}

}  // namespace internal
}  // namespace roo_display
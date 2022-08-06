#include "transformed.h"

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

Transform::Transform(bool xy_swap, int16_t x_scale, int16_t y_scale,
                     int16_t x_offset, int16_t y_offset)
    : x_scale_(x_scale),
      y_scale_(y_scale),
      x_offset_(x_offset),
      y_offset_(y_offset),
      xy_swap_(xy_swap),
      clipped_(false),
      clip_box_(Box::MaximumBox()) {}

Transform::Transform(bool xy_swap, int16_t x_scale, int16_t y_scale,
                     int16_t x_offset, int16_t y_offset, bool clipped,
                     Box clip_box)
    : x_scale_(x_scale),
      y_scale_(y_scale),
      x_offset_(x_offset),
      y_offset_(y_offset),
      xy_swap_(xy_swap),
      clipped_(clipped),
      clip_box_(std::move(clip_box)) {}

Transform::Transform() : Transform(false, 1, 1, 0, 0) {}

Transform Transform::swapXY() const {
  return Transform(!xy_swap_, y_scale_, x_scale_, y_offset_, x_offset_,
                   clipped_, clip_box_.swapXY());
}

Transform Transform::flipX() const {
  return Transform(xy_swap_, -x_scale_, y_scale_, -x_offset_, y_offset_,
                   clipped_, clip_box_.flipX());
}

Transform Transform::flipY() const {
  return Transform(xy_swap_, x_scale_, -y_scale_, x_offset_, -y_offset_,
                   clipped_, clip_box_.flipY());
}

Transform Transform::scale(int16_t x_scale, int16_t y_scale) const {
  return Transform(xy_swap_, x_scale_ * x_scale, y_scale_ * y_scale,
                   x_offset_ * x_scale, y_offset_ * y_scale, clipped_,
                   clip_box_.scale(x_scale, y_scale));
}

Transform Transform::translate(int16_t x_offset, int16_t y_offset) const {
  return Transform(xy_swap_, x_scale_, y_scale_, x_offset_ + x_offset,
                   y_offset_ + y_offset, clipped_,
                   clip_box_.translate(x_offset, y_offset));
}

Transform Transform::clip(Box clip_box) const {
  return Transform(xy_swap_, x_scale_, y_scale_, x_offset_, y_offset_, true,
                   clipped_ ? Box::intersect(clip_box_, clip_box) : clip_box);
}

Transform Transform::rotateUpsideDown() const {
  return Transform(xy_swap_, -x_scale_, -y_scale_, -x_offset_, -y_offset_,
                   clipped_, clip_box_.rotateUpsideDown());
}

Transform Transform::rotateRight() const {
  return Transform(!xy_swap_, -y_scale_, x_scale_, -y_offset_, x_offset_,
                   clipped_, clip_box_.rotateRight());
}

Transform Transform::rotateLeft() const {
  return Transform(!xy_swap_, y_scale_, -x_scale_, y_offset_, -x_offset_,
                   clipped_, clip_box_.rotateLeft());
}

Transform Transform::rotateClockwise(int turns) const {
  switch (turns & 3) {
    case 0:
      return *this;
    case 1:
      return rotateRight();
    case 2:
      return rotateUpsideDown();
    case 3:
    default:
      return rotateLeft();
  }
}

Transform Transform::rotateCounterClockwise(int turns) const {
  switch (turns & 3) {
    case 0:
      return *this;
    case 1:
      return rotateLeft();
    case 2:
      return rotateUpsideDown();
    case 3:
    default:
      return rotateRight();
  }
}

void Transform::transformRect(int16_t &x0, int16_t &y0, int16_t &x1,
                              int16_t &y1) const {
  x0 = x0 * x_scale_ + x_offset_;
  y0 = y0 * y_scale_ + y_offset_;
  x1 = (x1 + 1) * x_scale_ + x_offset_;
  y1 = (y1 + 1) * y_scale_ + y_offset_;
  if (x1 > x0) {
    --x1;
  } else {  // Must be x1 < x0.
    std::swap(x0, x1);
    ++x0;
  }
  if (y1 > y0) {
    --y1;
  } else {  // Must be y1 < y0.
    std::swap(y0, y1);
    ++y0;
  }
}

Box Transform::transformBox(Box in) const {
  int16_t x0 = in.xMin();
  int16_t y0 = in.yMin();
  int16_t x1 = in.xMax();
  int16_t y1 = in.yMax();
  transformRect(x0, y0, x1, y1);
  return Box(x0, y0, x1, y1);
}

int floor_div(int a, int b) {
  // https://stackoverflow.com/questions/3602827/what-is-the-behavior-of-integer-division
  int d = a / b;
  int r = a % b; /* optimizes into single division. */
  return r ? (d - ((a < 0) ^ (b < 0))) : d;
}

Box Transform::smallestEnclosingRect(const Box &rect) const {
  int16_t x0 = rect.xMin() - x_offset_;
  int16_t y0 = rect.yMin() - y_offset_;
  int16_t x1 = rect.xMax() - x_offset_;
  int16_t y1 = rect.yMax() - y_offset_;
  if (x_scale_ < 0) {
    std::swap(x0, x1);
  }
  x0 = floor_div(x0, x_scale_);
  x1 = floor_div(x1, x_scale_);
  if (y_scale_ < 0) {
    std::swap(y0, y1);
  }
  y0 = floor_div(y0, y_scale_);
  y1 = floor_div(y1, y_scale_);
  if (xy_swap_) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  return Box(x0, y0, x1, y1);
}

Box Transform::smallestBoundingRect() const {
  return clipped_ ? smallestEnclosingRect(clip_box_) : Box::MaximumBox();
}

void TransformedDisplayOutput::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                                          uint16_t y1, PaintMode mode) {
  if (!transform_.is_rescaled() && !transform_.xy_swap()) {
    delegate_.setAddress(x0 + transform_.x_offset(), y0 + transform_.y_offset(),
                         x1 + transform_.x_offset(), y1 + transform_.y_offset(),
                         mode);
  } else {
    addr_window_ = Box(x0, y0, x1, y1);
    paint_mode_ = mode;
    x_cursor_ = x0;
    y_cursor_ = y0;
  }
}

void TransformedDisplayOutput::write(Color *color, uint32_t pixel_count) {
  if (!transform_.is_rescaled() && !transform_.xy_swap()) {
    delegate_.write(color, pixel_count);
  } else {
    ClippingBufferedRectWriter writer(delegate_, clip_box_, paint_mode_);
    while (pixel_count-- > 0) {
      int16_t x0 = x_cursor_;
      int16_t y0 = y_cursor_;
      if (transform_.xy_swap()) {
        std::swap(x0, y0);
      }
      int16_t x1 = x0;
      int16_t y1 = y0;
      transform_.transformRect(x0, y0, x1, y1);
      writer.writeRect(x0, y0, x1, y1, *color++);
      if (x_cursor_ < addr_window_.xMax()) {
        ++x_cursor_;
      } else {
        x_cursor_ = addr_window_.xMin();
        ++y_cursor_;
      }
    }
  }
}

// virtual void fill(PaintMode mode, Color color, uint32_t pixel_count) = 0;

void TransformedDisplayOutput::writePixels(PaintMode mode, Color *color,
                                           int16_t *x, int16_t *y,
                                           uint16_t pixel_count) {
  if (transform_.xy_swap()) {
    std::swap(x, y);
  }
  if (!transform_.is_rescaled()) {
    if (!transform_.is_translated()) {
      delegate_.writePixels(mode, color, x, y, pixel_count);
    } else {
      ClippingBufferedPixelWriter writer(delegate_, clip_box_, mode);
      int16_t dx = transform_.x_offset();
      int16_t dy = transform_.y_offset();
      if (transform_.xy_swap()) {
        std::swap(dx, dy);
      }
      while (pixel_count-- > 0) {
        int16_t ix = *x++ + dx;
        int16_t iy = *y++ + dy;
        writer.writePixel(ix, iy, *color++);
      }
    }
  } else {
    ClippingBufferedRectWriter writer(delegate_, clip_box_, mode);
    while (pixel_count-- > 0) {
      int16_t x0 = *x++;
      int16_t y0 = *y++;
      int16_t x1 = x0;
      int16_t y1 = y0;
      transform_.transformRect(x0, y0, x1, y1);
      writer.writeRect(x0, y0, x1, y1, *color++);
    }
  }
}

void TransformedDisplayOutput::fillPixels(PaintMode mode, Color color,
                                          int16_t *x, int16_t *y,
                                          uint16_t pixel_count) {
  if (transform_.xy_swap()) {
    std::swap(x, y);
  }
  if (!transform_.is_rescaled()) {
    if (!transform_.is_translated()) {
      delegate_.fillPixels(mode, color, x, y, pixel_count);
    } else {
      ClippingBufferedPixelFiller filler(delegate_, color, clip_box_, mode);
      while (pixel_count-- > 0) {
        int16_t ix = *x++ + transform_.x_offset();
        int16_t iy = *y++ + transform_.y_offset();
        filler.fillPixel(ix, iy);
      }
    }
  } else {
    ClippingBufferedRectFiller filler(delegate_, color, clip_box_, mode);
    while (pixel_count-- > 0) {
      int16_t x0 = *x++;
      int16_t y0 = *y++;
      int16_t x1 = x0;
      int16_t y1 = y0;
      transform_.transformRect(x0, y0, x1, y1);
      filler.fillRect(x0, y0, x1, y1);
    }
  }
}

void TransformedDisplayOutput::writeRects(PaintMode mode, Color *color,
                                          int16_t *x0, int16_t *y0, int16_t *x1,
                                          int16_t *y1, uint16_t count) {
  ClippingBufferedRectWriter writer(delegate_, clip_box_, mode);
  if (transform_.xy_swap()) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  while (count-- > 0) {
    int16_t xMin = *x0++;
    int16_t yMin = *y0++;
    int16_t xMax = *x1++;
    int16_t yMax = *y1++;
    transform_.transformRect(xMin, yMin, xMax, yMax);
    writer.writeRect(xMin, yMin, xMax, yMax, *color++);
  }
}

void TransformedDisplayOutput::fillRects(PaintMode mode, Color color,
                                         int16_t *x0, int16_t *y0, int16_t *x1,
                                         int16_t *y1, uint16_t count) {
  ClippingBufferedRectFiller filler(delegate_, color, clip_box_, mode);
  if (transform_.xy_swap()) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  while (count-- > 0) {
    int16_t xMin = *x0++;
    int16_t yMin = *y0++;
    int16_t xMax = *x1++;
    int16_t yMax = *y1++;
    transform_.transformRect(xMin, yMin, xMax, yMax);
    filler.fillRect(xMin, yMin, xMax, yMax);
  }
}

}  // namespace roo_display

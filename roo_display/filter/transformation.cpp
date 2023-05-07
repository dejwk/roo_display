#include "transformation.h"

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

Transformation::Transformation(bool xy_swap, int16_t x_scale, int16_t y_scale,
                               int16_t x_offset, int16_t y_offset)
    : x_scale_(x_scale),
      y_scale_(y_scale),
      x_offset_(x_offset),
      y_offset_(y_offset),
      xy_swap_(xy_swap),
      clipped_(false),
      clip_box_(Box::MaximumBox()) {}

Transformation::Transformation(bool xy_swap, int16_t x_scale, int16_t y_scale,
                               int16_t x_offset, int16_t y_offset, bool clipped,
                               Box clip_box)
    : x_scale_(x_scale),
      y_scale_(y_scale),
      x_offset_(x_offset),
      y_offset_(y_offset),
      xy_swap_(xy_swap),
      clipped_(clipped),
      clip_box_(std::move(clip_box)) {}

Transformation::Transformation() : Transformation(false, 1, 1, 0, 0) {}

Transformation Transformation::swapXY() const {
  return Transformation(!xy_swap_, y_scale_, x_scale_, y_offset_, x_offset_,
                        clipped_, clip_box_.swapXY());
}

Transformation Transformation::flipX() const {
  return Transformation(xy_swap_, -x_scale_, y_scale_, -x_offset_, y_offset_,
                        clipped_, clip_box_.flipX());
}

Transformation Transformation::flipY() const {
  return Transformation(xy_swap_, x_scale_, -y_scale_, x_offset_, -y_offset_,
                        clipped_, clip_box_.flipY());
}

Transformation Transformation::scale(int16_t x_scale, int16_t y_scale) const {
  return Transformation(xy_swap_, x_scale_ * x_scale, y_scale_ * y_scale,
                        x_offset_ * x_scale, y_offset_ * y_scale, clipped_,
                        clip_box_.scale(x_scale, y_scale));
}

Transformation Transformation::translate(int16_t x_offset,
                                         int16_t y_offset) const {
  return Transformation(xy_swap_, x_scale_, y_scale_, x_offset_ + x_offset,
                        y_offset_ + y_offset, clipped_,
                        clip_box_.translate(x_offset, y_offset));
}

Transformation Transformation::clip(Box clip_box) const {
  return Transformation(
      xy_swap_, x_scale_, y_scale_, x_offset_, y_offset_, true,
      clipped_ ? Box::Intersect(clip_box_, clip_box) : clip_box);
}

Transformation Transformation::rotateUpsideDown() const {
  return Transformation(xy_swap_, -x_scale_, -y_scale_, -x_offset_, -y_offset_,
                        clipped_, clip_box_.rotateUpsideDown());
}

Transformation Transformation::rotateRight() const {
  return Transformation(!xy_swap_, -y_scale_, x_scale_, -y_offset_, x_offset_,
                        clipped_, clip_box_.rotateRight());
}

Transformation Transformation::rotateLeft() const {
  return Transformation(!xy_swap_, y_scale_, -x_scale_, y_offset_, -x_offset_,
                        clipped_, clip_box_.rotateLeft());
}

Transformation Transformation::rotateClockwise(int turns) const {
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

Transformation Transformation::rotateCounterClockwise(int turns) const {
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

void Transformation::transformRectNoSwap(int16_t &x0, int16_t &y0, int16_t &x1,
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

Box Transformation::transformBox(Box in) const {
  int16_t x0 = in.xMin();
  int16_t y0 = in.yMin();
  int16_t x1 = in.xMax();
  int16_t y1 = in.yMax();
  if (xy_swap_) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  transformRectNoSwap(x0, y0, x1, y1);
  return Box(x0, y0, x1, y1);
}

int floor_div(int a, int b) {
  // https://stackoverflow.com/questions/3602827/what-is-the-behavior-of-integer-division
  int d = a / b;
  int r = a % b; /* optimizes into single division. */
  return r ? (d - ((a < 0) ^ (b < 0))) : d;
}

Box Transformation::smallestEnclosingRect(const Box &rect) const {
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

Box Transformation::smallestBoundingRect() const {
  return clipped_ ? smallestEnclosingRect(clip_box_) : Box::MaximumBox();
}

void TransformedDisplayOutput::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                                          uint16_t y1, BlendingMode mode) {
  if (!transformation_.is_rescaled() && !transformation_.xy_swap()) {
    delegate_.setAddress(
        x0 + transformation_.x_offset(), y0 + transformation_.y_offset(),
        x1 + transformation_.x_offset(), y1 + transformation_.y_offset(), mode);
  } else {
    addr_window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    x_cursor_ = x0;
    y_cursor_ = y0;
  }
}

void TransformedDisplayOutput::write(Color *color, uint32_t pixel_count) {
  if (!transformation_.is_rescaled() && !transformation_.xy_swap()) {
    delegate_.write(color, pixel_count);
  } else if (!transformation_.is_abs_rescaled()) {
    ClippingBufferedPixelWriter writer(delegate_, clip_box_, blending_mode_);
    while (pixel_count-- > 0) {
      int16_t x = x_cursor_;
      int16_t y = y_cursor_;
      if (transformation_.xy_swap()) {
        std::swap(x, y);
      }
      writer.writePixel(
          x * transformation_.x_scale() + transformation_.x_offset(),
          y * transformation_.y_scale() + transformation_.y_offset(), *color++);
      if (x_cursor_ < addr_window_.xMax()) {
        ++x_cursor_;
      } else {
        x_cursor_ = addr_window_.xMin();
        ++y_cursor_;
      }
    }
  } else {
    ClippingBufferedRectWriter writer(delegate_, clip_box_, blending_mode_);
    while (pixel_count-- > 0) {
      int16_t x0 = x_cursor_;
      int16_t y0 = y_cursor_;
      if (transformation_.xy_swap()) {
        std::swap(x0, y0);
      }
      int16_t x1 = x0;
      int16_t y1 = y0;
      transformation_.transformRectNoSwap(x0, y0, x1, y1);
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

// virtual void fill(BlendingMode mode, Color color, uint32_t pixel_count) = 0;

void TransformedDisplayOutput::writePixels(BlendingMode mode, Color *color,
                                           int16_t *x, int16_t *y,
                                           uint16_t pixel_count) {
  if (transformation_.xy_swap()) {
    std::swap(x, y);
  }
  if (!transformation_.is_rescaled()) {
    if (!transformation_.is_translated()) {
      delegate_.writePixels(mode, color, x, y, pixel_count);
    } else {
      ClippingBufferedPixelWriter writer(delegate_, clip_box_, mode);
      int16_t dx = transformation_.x_offset();
      int16_t dy = transformation_.y_offset();
      while (pixel_count-- > 0) {
        int16_t ix = *x++ + dx;
        int16_t iy = *y++ + dy;
        writer.writePixel(ix, iy, *color++);
      }
    }
  } else if (!transformation_.is_abs_rescaled()) {
    ClippingBufferedPixelWriter writer(delegate_, clip_box_, mode);
    while (pixel_count-- > 0) {
      int16_t ix =
          transformation_.x_scale() * *x++ + transformation_.x_offset();
      int16_t iy =
          transformation_.y_scale() * *y++ + transformation_.y_offset();
      writer.writePixel(ix, iy, *color++);
    }
  } else {
    ClippingBufferedRectWriter writer(delegate_, clip_box_, mode);
    while (pixel_count-- > 0) {
      int16_t x0 = *x++;
      int16_t y0 = *y++;
      int16_t x1 = x0;
      int16_t y1 = y0;
      transformation_.transformRectNoSwap(x0, y0, x1, y1);
      writer.writeRect(x0, y0, x1, y1, *color++);
    }
  }
}

void TransformedDisplayOutput::fillPixels(BlendingMode mode, Color color,
                                          int16_t *x, int16_t *y,
                                          uint16_t pixel_count) {
  if (transformation_.xy_swap()) {
    std::swap(x, y);
  }
  if (!transformation_.is_rescaled()) {
    if (!transformation_.is_translated()) {
      delegate_.fillPixels(mode, color, x, y, pixel_count);
    } else {
      ClippingBufferedPixelFiller filler(delegate_, color, clip_box_, mode);
      while (pixel_count-- > 0) {
        int16_t ix = *x++ + transformation_.x_offset();
        int16_t iy = *y++ + transformation_.y_offset();
        filler.fillPixel(ix, iy);
      }
    }
  } else if (!transformation_.is_abs_rescaled()) {
    ClippingBufferedPixelFiller filler(delegate_, color, clip_box_, mode);
    while (pixel_count-- > 0) {
      int16_t ix =
          transformation_.x_scale() * *x++ + transformation_.x_offset();
      int16_t iy =
          transformation_.y_scale() * *y++ + transformation_.y_offset();
      filler.fillPixel(ix, iy);
    }
  } else {
    ClippingBufferedRectFiller filler(delegate_, color, clip_box_, mode);
    while (pixel_count-- > 0) {
      int16_t x0 = *x++;
      int16_t y0 = *y++;
      int16_t x1 = x0;
      int16_t y1 = y0;
      transformation_.transformRectNoSwap(x0, y0, x1, y1);
      filler.fillRect(x0, y0, x1, y1);
    }
  }
}

void TransformedDisplayOutput::writeRects(BlendingMode mode, Color *color,
                                          int16_t *x0, int16_t *y0, int16_t *x1,
                                          int16_t *y1, uint16_t count) {
  ClippingBufferedRectWriter writer(delegate_, clip_box_, mode);
  if (transformation_.xy_swap()) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  while (count-- > 0) {
    int16_t xMin = *x0++;
    int16_t yMin = *y0++;
    int16_t xMax = *x1++;
    int16_t yMax = *y1++;
    transformation_.transformRectNoSwap(xMin, yMin, xMax, yMax);
    writer.writeRect(xMin, yMin, xMax, yMax, *color++);
  }
}

void TransformedDisplayOutput::fillRects(BlendingMode mode, Color color,
                                         int16_t *x0, int16_t *y0, int16_t *x1,
                                         int16_t *y1, uint16_t count) {
  ClippingBufferedRectFiller filler(delegate_, color, clip_box_, mode);
  if (transformation_.xy_swap()) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  while (count-- > 0) {
    int16_t xMin = *x0++;
    int16_t yMin = *y0++;
    int16_t xMax = *x1++;
    int16_t yMax = *y1++;
    transformation_.transformRectNoSwap(xMin, yMin, xMax, yMax);
    filler.fillRect(xMin, yMin, xMax, yMax);
  }
}

}  // namespace roo_display

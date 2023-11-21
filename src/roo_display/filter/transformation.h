#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

class Transformation {
 public:
  Transformation(bool xy_swap, int16_t x_scale, int16_t y_scale,
                 int16_t y_offset, int16_t x_offset);

  Transformation(bool xy_swap, int16_t x_scale, int16_t y_scale,
                 int16_t y_offset, int16_t x_offset, bool clipped,
                 Box clip_rect);

  Transformation();

  Transformation swapXY() const;
  Transformation flipX() const;
  Transformation flipY() const;
  Transformation scale(int16_t x_scale, int16_t y_scale) const;
  Transformation translate(int16_t x_offset, int16_t y_offset) const;
  Transformation clip(Box clip_box) const;
  Transformation rotateRight() const;
  Transformation rotateLeft() const;
  Transformation rotateUpsideDown() const;
  Transformation rotateClockwise(int turns) const;
  Transformation rotateCounterClockwise(int turns) const;

  bool xy_swap() const { return xy_swap_; }
  int16_t x_scale() const { return x_scale_; }
  int16_t y_scale() const { return y_scale_; }
  int16_t x_offset() const { return x_offset_; }
  int16_t y_offset() const { return y_offset_; }
  bool clipped() const { return clipped_; }
  Box clip_box() const { return clipped_ ? clip_box_ : Box::MaximumBox(); }

  // Returns whether the object has scale different than 1 along any axes.
  bool is_rescaled() const { return x_scale_ != 1 || y_scale_ != 1; }

  // Returns whether the object has scale different than 1 or -1 along any axes.
  bool is_abs_rescaled() const {
    return (x_scale_ != 1 && x_scale_ != -1) ||
           (y_scale_ != 1 && y_scale_ != -1);
  }

  bool is_translated() const { return x_offset_ != 0 || y_offset_ != 0; }

  // Applies the transformation to the specified rectangle, assuming that if the
  // Transformation has swapped coordinates, the rectangle comes with
  // coordinates already swapped.
  void transformRectNoSwap(int16_t &x0, int16_t &y0, int16_t &x1,
                           int16_t &y1) const;

  // Applies the transformation the specified box.
  Box transformBox(Box in) const;

  // Calculates the smallest rectangle in the destination (screen) coordinates
  // that will completely enclose the given `rect` expressed in the object
  // (non-transformed) coordinates.
  Box smallestEnclosingRect(const Box &rect) const;

  // Returns the smallest bounding rectangle in the destination (screen)
  // coordinates that will cover the clip box of the transformation, which is
  // expresses in the object (non-translated) coordinates.
  Box smallestBoundingRect() const;

 private:
  int16_t x_scale_;
  int16_t y_scale_;
  int16_t x_offset_;
  int16_t y_offset_;
  bool xy_swap_;
  bool clipped_;
  Box clip_box_;
};

class TransformedDisplayOutput : public DisplayOutput {
 public:
  TransformedDisplayOutput(DisplayOutput &delegate,
                           Transformation transformation)
      : delegate_(delegate),
        transformation_(std::move(transformation)),
        clip_box_(transformation_.clip_box()),
        addr_window_(),
        x_cursor_(0),
        y_cursor_(0) {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override;

  void write(Color *color, uint32_t pixel_count) override;

  // virtual void fill(BlendingMode mode, Color color, uint32_t pixel_count) = 0;

  void writePixels(BlendingMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(BlendingMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(BlendingMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t pixel_count) override;

  void fillRects(BlendingMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override;

  const Box &clip_box() const { return clip_box_; }

 private:
  DisplayOutput &delegate_;
  Transformation transformation_;
  Box clip_box_;
  Box addr_window_;
  BlendingMode blending_mode_;
  int16_t x_cursor_;
  int16_t y_cursor_;
};

class TransformedDrawable : public Drawable {
 public:
  TransformedDrawable(Transformation transformation, const Drawable *delegate)
      : transformation_(std::move(transformation)), delegate_(delegate) {}

  Box extents() const override {
    return transformation_.transformBox(delegate_->extents());
  }

  Box anchorExtents() const override {
    return transformation_.transformBox(delegate_->anchorExtents());
  }

 private:
  void drawTo(const Surface &s) const override {
    Transformation adjusted =
        transformation_.translate(s.dx(), s.dy()).clip(s.clip_box());
    TransformedDisplayOutput new_output(s.out(), adjusted);
    Surface news(&new_output, adjusted.smallestBoundingRect(),
                 s.is_write_once(), s.bgcolor(), s.fill_mode(), s.blending_mode());
    news.drawObject(*delegate_);
  }

 private:
  Transformation transformation_;
  const Drawable *delegate_;
};

}  // namespace roo_display
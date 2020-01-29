#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

class Transform {
 public:
  Transform(bool xy_swap, int16_t x_scale, int16_t y_scale, int16_t y_offset,
            int16_t x_offset);

  Transform(bool xy_swap, int16_t x_scale, int16_t y_scale, int16_t y_offset,
            int16_t x_offset, bool clipped, Box clip_rect);

  Transform();

  Transform swapXY() const;
  Transform flipX() const;
  Transform flipY() const;
  Transform scale(int16_t x_scale, int16_t y_scale) const;
  Transform translate(int16_t x_offset, int16_t y_offset) const;
  Transform clip(Box clip_box) const;
  Transform rotateRight() const;
  Transform rotateLeft() const;
  Transform rotateUpsideDown() const;
  Transform rotateClockwise(int turns) const;
  Transform rotateCounterClockwise(int turns) const;

  bool xy_swap() const { return xy_swap_; }
  int16_t x_scale() const { return x_scale_; }
  int16_t y_scale() const { return y_scale_; }
  int16_t x_offset() const { return x_offset_; }
  int16_t y_offset() const { return y_offset_; }
  bool clipped() const { return clipped_; }
  Box clip_box() const { return clipped_ ? clip_box_ : Box::MaximumBox(); }

  bool is_rescaled() const { return x_scale_ != 1 || y_scale_ != 1; }
  bool is_translated() const { return x_offset_ != 0 || y_offset_ != 0; }

  void transformRect(int16_t &x0, int16_t &y0, int16_t &x1, int16_t &y1) const;

  Box smallestEnclosingRect(const Box &rect) const;
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
  TransformedDisplayOutput(DisplayOutput *delegate, Transform transform)
      : delegate_(delegate),
        transform_(std::move(transform)),
        clip_box_(transform_.clip_box()),
        addr_window_(),
        x_cursor_(0),
        y_cursor_(0) {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) override;

  void write(PaintMode mode, Color *color, uint32_t pixel_count) override;

  // virtual void fill(PaintMode mode, Color color, uint32_t pixel_count) = 0;

  void writePixels(PaintMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(PaintMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(PaintMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t pixel_count) override;

  void fillRects(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override;

  const Box& clip_box() const { return clip_box_; }

 private:
  DisplayOutput *delegate_;
  Transform transform_;
  Box clip_box_;
  Box addr_window_;
  int16_t x_cursor_;
  int16_t y_cursor_;
};

class TransformedDrawable : public Drawable {
 public:
  TransformedDrawable(Transform transform, const Drawable *delegate)
      : transform_(std::move(transform)), delegate_(delegate) {}

  Box extents() const override {
    Box base = delegate_->extents();
    int16_t x0 = base.xMin();
    int16_t y0 = base.yMin();
    int16_t x1 = base.xMax();
    int16_t y1 = base.yMax();
    transform_.transformRect(x0, y0, x1, y1);
    return Box(x0, y0, x1, y1);
  }

 private:
  void drawTo(const Surface& s) const override {
    Transform adjusted = transform_.translate(s.dx, s.dy).clip(s.clip_box);
    TransformedDisplayOutput new_output(s.out, adjusted);
    Surface news(&new_output, adjusted.smallestBoundingRect(), s.bgcolor);
    news.drawObject(*delegate_);
  }

 private:
  Transform transform_;
  const Drawable *delegate_;
};

}  // namespace roo_display
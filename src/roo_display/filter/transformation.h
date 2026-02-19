#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

/// Geometric transformation: swap, scale, translate, rotate, clip.
class Transformation {
 public:
  /// Construct a transformation with swap, scale, and translation.
  Transformation(bool xy_swap, int16_t x_scale, int16_t y_scale,
                 int16_t y_offset, int16_t x_offset);

  /// Construct a transformation with optional clip box.
  Transformation(bool xy_swap, int16_t x_scale, int16_t y_scale,
                 int16_t y_offset, int16_t x_offset, bool clipped,
                 Box clip_rect);

  /// Identity transformation.
  Transformation();

  /// Swap x/y axes.
  Transformation swapXY() const;
  /// Flip across the Y axis.
  Transformation flipX() const;
  /// Flip across the X axis.
  Transformation flipY() const;
  /// Scale along both axes.
  Transformation scale(int16_t x_scale, int16_t y_scale) const;
  /// Translate by offsets.
  Transformation translate(int16_t x_offset, int16_t y_offset) const;
  /// Intersect with a clip box.
  Transformation clip(Box clip_box) const;
  /// Rotate 90 degrees clockwise.
  Transformation rotateRight() const;
  /// Rotate 90 degrees counter-clockwise.
  Transformation rotateLeft() const;
  /// Rotate 180 degrees.
  Transformation rotateUpsideDown() const;
  /// Rotate clockwise by `turns` (multiples of 90 degrees).
  Transformation rotateClockwise(int turns) const;
  /// Rotate counter-clockwise by `turns` (multiples of 90 degrees).
  Transformation rotateCounterClockwise(int turns) const;

  /// Whether x/y axes are swapped.
  bool xy_swap() const { return xy_swap_; }
  /// X scale factor.
  int16_t x_scale() const { return x_scale_; }
  /// Y scale factor.
  int16_t y_scale() const { return y_scale_; }
  /// X translation.
  int16_t x_offset() const { return x_offset_; }
  /// Y translation.
  int16_t y_offset() const { return y_offset_; }
  /// Whether clipping is enabled.
  bool clipped() const { return clipped_; }
  /// Effective clip box.
  Box clip_box() const { return clipped_ ? clip_box_ : Box::MaximumBox(); }

  /// Returns true if scale differs from 1 on any axis.
  bool is_rescaled() const { return x_scale_ != 1 || y_scale_ != 1; }

  /// Returns true if scale differs from 1 or -1 on any axis.
  bool is_abs_rescaled() const {
    return (x_scale_ != 1 && x_scale_ != -1) ||
           (y_scale_ != 1 && y_scale_ != -1);
  }

  /// Returns true if translation is non-zero.
  bool is_translated() const { return x_offset_ != 0 || y_offset_ != 0; }

  /// Apply transformation to a rectangle without swapping input coordinates.
  ///
  /// If the transformation swaps axes, the input rectangle is assumed already
  /// swapped.
  void transformRectNoSwap(int16_t &x0, int16_t &y0, int16_t &x1,
                           int16_t &y1) const;

  /// Apply the transformation to a box.
  Box transformBox(Box in) const;

  /// Smallest rectangle in destination coordinates enclosing `rect`.
  Box smallestEnclosingRect(const Box &rect) const;

  /// Smallest bounding rect covering the clip box in destination coordinates.
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

/// Display output wrapper that applies a `Transformation`.
class TransformedDisplayOutput : public DisplayOutput {
 public:
  /// Construct a transformed output.
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

  void fill(Color color, uint32_t pixel_count) override;

  void writePixels(BlendingMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(BlendingMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(BlendingMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t pixel_count) override;

  void fillRects(BlendingMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override;

  /// Return the effective clip box.
  const Box &clip_box() const { return clip_box_; }

  const ColorFormat &getColorFormat() const override {
    return delegate_.getColorFormat();
  }

 private:
  DisplayOutput &delegate_;
  Transformation transformation_;
  Box clip_box_;
  Box addr_window_;
  BlendingMode blending_mode_;
  int16_t x_cursor_;
  int16_t y_cursor_;
};

/// Drawable wrapper that applies a `Transformation`.
class TransformedDrawable : public Drawable {
 public:
  /// Construct a transformed drawable.
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
                 s.is_write_once(), s.bgcolor(), s.fill_mode(),
                 s.blending_mode());
    news.drawObject(*delegate_);
  }

 private:
  Transformation transformation_;
  const Drawable *delegate_;
};

}  // namespace roo_display
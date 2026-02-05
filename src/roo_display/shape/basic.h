#pragma once

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/shape/point.h"

namespace roo_display {

/// Base class for simple colored shapes.
class BasicShape : virtual public Drawable {
 public:
  /// Return the shape color.
  Color color() const { return color_; }

 protected:
  BasicShape(Color color) : color_(color) {}

 private:
  Color color_;
};

/// A straight line segment between two points.
class Line : public BasicShape {
 public:
  Line(Point a, Point b, Color color) : Line(a.x, a.y, b.x, b.y, color) {}

  Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color)
      : BasicShape(color), x0_(x0), y0_(y0), x1_(x1), y1_(y1), diag_(TL_BR) {
    if (x1 < x0) {
      std::swap(x0_, x1_);
      diag_ = diag_ == TL_BR ? TR_BL : TL_BR;
    }
    if (y1 < y0) {
      std::swap(y0_, y1_);
      diag_ = diag_ == TL_BR ? TR_BL : TL_BR;
    }
  }

  Box extents() const override { return Box(x0_, y0_, x1_, y1_); }

 private:
  enum Diagonal { TL_BR = 0, TR_BL = 1 };

  void drawTo(const Surface &s) const override;

  bool steep() const { return y1_ - y0_ > x1_ - x0_; }

  int16_t x0_;
  int16_t y0_;
  int16_t x1_;
  int16_t y1_;
  Diagonal diag_;
};

/// Base class for axis-aligned rectangle shapes.
class RectBase : public BasicShape {
 public:
  RectBase(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color)
      : BasicShape(color), x0_(x0), y0_(y0), x1_(x1), y1_(y1) {
    if (x1 < x0) std::swap(x0_, x1_);
    if (y1 < y0) std::swap(y0_, y1_);
  }

  Box extents() const override { return Box(x0_, y0_, x1_, y1_); }

 protected:
  int16_t x0_;
  int16_t y0_;
  int16_t x1_;
  int16_t y1_;
};

/// Outline rectangle.
class Rect : public RectBase {
 public:
  Rect(const Box &box, Color color)
      : RectBase(box.xMin(), box.yMin(), box.xMax(), box.yMax(), color) {}

  Rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color)
      : RectBase(x0, y0, x1, y1, color) {}

 private:
  void drawTo(const Surface &s) const override;
};

/// Rectangle border with configurable thickness on each side.
class Border : public RectBase {
 public:
  Border(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t border,
         Color color)
      : Border(x0, y0, x1, y1, border, border, border, border, color) {}

  Border(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t hborder,
         int16_t vborder, Color color)
      : Border(x0, y0, x1, y1, hborder, vborder, hborder, vborder, color) {}

  Border(const roo_display::Box &outer, const roo_display::Box &inner,
         Color color)
      : Border(outer.xMin(), outer.yMin(), outer.xMax(), outer.yMax(),
               inner.xMin() - outer.xMin(), inner.yMin() - outer.yMin(),
               outer.xMax() - inner.xMax(), outer.yMax() - inner.yMax(),
               color) {}

  Border(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t left,
         int16_t top, int16_t right, int16_t bottom, Color color)
      : RectBase(x0, y0, x1, y1, color),
        left_(left < 0 ? 0 : left),
        top_(top < 0 ? 0 : top),
        right_(right < 0 ? 0 : right),
        bottom_(bottom < 0 ? 0 : bottom) {
    if ((x0_ + left_ + 1 >= x1_ - right_) ||
        (y0_ + top_ + 1 >= y1_ - bottom_)) {
      // Full filled rect.
      left_ = x1_ - x0_ + 1;
      right_ = 0;
      top_ = 0;
      bottom_ = 0;
    }
  }

 private:
  int16_t left_;
  int16_t top_;
  int16_t right_;
  int16_t bottom_;
  void drawTo(const Surface &s) const override;
};

/// Filled rectangle (rasterizable).
class FilledRect : public RectBase, public Rasterizable {
 public:
  FilledRect(const Box &box, Color color)
      : RectBase(box.xMin(), box.yMin(), box.xMax(), box.yMax(), color) {}

  FilledRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color)
      : RectBase(x0, y0, x1, y1, color) {}

  Box extents() const override { return RectBase::extents(); }

  TransparencyMode getTransparencyMode() const override {
    return color().isOpaque() ? TRANSPARENCY_NONE : TRANSPARENCY_GRADUAL;
  }

  std::unique_ptr<PixelStream> createStream() const override;
  std::unique_ptr<PixelStream> createStream(const Box &bounds) const override;

  void readColors(const int16_t *x, const int16_t *y, uint32_t count,
                  Color *result) const override {
    FillColor(result, count, color());
  }

 private:
  void drawTo(const Surface &s) const override;
};

/// Base class for rounded rectangles.
class RoundRectBase : public BasicShape {
 public:
  RoundRectBase(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t radius,
                Color color)
      : BasicShape(color), x0_(x0), y0_(y0), x1_(x1), y1_(y1), radius_(radius) {
    if (x1 < x0) std::swap(x0_, x1_);
    if (y1 < y0) std::swap(y0_, y1_);
    int16_t w = x1 - x0 + 1;
    int16_t h = y1 - y0 + 1;
    int16_t max_radius = ((w < h) ? w : h) / 2;  // 1/2 minor axis
    if (radius_ > max_radius) radius_ = max_radius;
  }

  Box extents() const override { return Box(x0_, y0_, x1_, y1_); }

 protected:
  int16_t x0_;
  int16_t y0_;
  int16_t x1_;
  int16_t y1_;
  int16_t radius_;
};

/// Outline rounded rectangle.
class RoundRect : public RoundRectBase {
 public:
  RoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t radius,
            Color color)
      : RoundRectBase(x0, y0, x1, y1, radius, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

/// Filled rounded rectangle.
class FilledRoundRect : public RoundRectBase {
 public:
  FilledRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t radius, Color color)
      : RoundRectBase(x0, y0, x1, y1, radius, color) {}

 private:
  void drawTo(const Surface &s) const override;
};

/// Base class for circles.
class CircleBase : public BasicShape {
 public:
  Box extents() const override {
    return Box(x0_, y0_, x0_ + diameter_ - 1, y0_ + diameter_ - 1);
  }

 protected:
  CircleBase(int16_t x0, int16_t y0, int16_t diameter, Color color)
      : BasicShape(color), x0_(x0), y0_(y0), diameter_(diameter) {}

  int16_t x0_;
  int16_t y0_;
  int16_t diameter_;
};

/// Outline circle.
class Circle : public CircleBase {
 public:
  /// Create an outline circle by center point and radius.
  ///
  /// @param center Circle center.
  /// @param radius Radius in pixels.
  /// @param color Outline color.
  static Circle ByRadius(Point center, int16_t radius, Color color) {
    return ByRadius(center.x, center.y, radius, color);
  }

  /// Create an outline circle by center coordinates and radius.
  ///
  /// @param x_center Center x-coordinate.
  /// @param y_center Center y-coordinate.
  /// @param radius Radius in pixels.
  /// @param color Outline color.
  static Circle ByRadius(int16_t x_center, int16_t y_center, int16_t radius,
                         Color color) {
    return Circle(x_center - radius, y_center - radius, (radius << 1) + 1,
                  color);
  }

  /// Create an outline circle by top-left and diameter.
  ///
  /// @param top_left Top-left corner of the bounding square.
  /// @param diameter Diameter in pixels.
  /// @param color Outline color.
  static Circle ByExtents(Point top_left, int16_t diameter, Color color) {
    return ByExtents(top_left.x, top_left.y, diameter, color);
  }

  /// Create an outline circle by top-left coordinates and diameter.
  ///
  /// @param x0 Top-left x-coordinate of the bounding square.
  /// @param y0 Top-left y-coordinate of the bounding square.
  /// @param diameter Diameter in pixels.
  /// @param color Outline color.
  static Circle ByExtents(int16_t x0, int16_t y0, int16_t diameter,
                          Color color) {
    return Circle(x0, y0, diameter, color);
  }

 private:
  Circle(int16_t x0, int16_t y0, int16_t diameter, Color color)
      : CircleBase(x0, y0, diameter, color) {}

  void drawInteriorTo(const Surface &s) const override;
};

/// Filled circle.
class FilledCircle : public CircleBase {
 public:
  /// Create a filled circle by center point and radius.
  ///
  /// @param center Circle center.
  /// @param radius Radius in pixels.
  /// @param color Fill color.
  static FilledCircle ByRadius(Point center, int16_t radius, Color color) {
    return ByRadius(center.x, center.y, radius, color);
  }

  /// Create a filled circle by center coordinates and radius.
  ///
  /// @param x_center Center x-coordinate.
  /// @param y_center Center y-coordinate.
  /// @param radius Radius in pixels.
  /// @param color Fill color.
  static FilledCircle ByRadius(int16_t x_center, int16_t y_center,
                               int16_t radius, Color color) {
    return FilledCircle(x_center - radius, y_center - radius, (radius << 1) + 1,
                        color);
  }

  /// Create a filled circle by top-left and diameter.
  ///
  /// @param top_left Top-left corner of the bounding square.
  /// @param diameter Diameter in pixels.
  /// @param color Fill color.
  static FilledCircle ByExtents(Point top_left, int16_t diameter, Color color) {
    return ByExtents(top_left.x, top_left.y, diameter, color);
  }

  /// Create a filled circle by top-left coordinates and diameter.
  ///
  /// @param x0 Top-left x-coordinate of the bounding square.
  /// @param y0 Top-left y-coordinate of the bounding square.
  /// @param diameter Diameter in pixels.
  /// @param color Fill color.
  static FilledCircle ByExtents(int16_t x0, int16_t y0, int16_t diameter,
                                Color color) {
    return FilledCircle(x0, y0, diameter, color);
  }

 private:
  FilledCircle(int16_t x0, int16_t y0, int16_t diameter, Color color)
      : CircleBase(x0, y0, diameter, color) {}

  void drawTo(const Surface &s) const override;
};

class TriangleBase : public BasicShape {
 public:
  TriangleBase(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
               int16_t y2, Color color)
      : BasicShape(color),
        x0_(x0),
        y0_(y0),
        x1_(x1),
        y1_(y1),
        x2_(x2),
        y2_(y2) {
    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0_ > y1_) {
      std::swap(y0_, y1_);
      std::swap(x0_, x1_);
    }
    if (y1_ > y2_) {
      std::swap(y2_, y1_);
      std::swap(x2_, x1_);
    }
    if (y0_ > y1_) {
      std::swap(y0_, y1_);
      std::swap(x0_, x1_);
    }
  }

  Box extents() const override {
    return Box(std::min(x0_, std::min(x1_, x2_)), y0_,
               std::max(x0_, std::max(x1_, x2_)), y2_);
  }

 protected:
  int16_t x0_;
  int16_t y0_;
  int16_t x1_;
  int16_t y1_;
  int16_t x2_;
  int16_t y2_;
};

class Triangle : public TriangleBase {
 public:
  Triangle(Point a, Point b, Point c, Color color)
      : Triangle(a.x, a.y, b.x, b.y, c.x, c.y, color) {}

  Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
           int16_t y2, Color color)
      : TriangleBase(x0, y0, x1, y1, x2, y2, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

class FilledTriangle : public TriangleBase {
 public:
  FilledTriangle(Point a, Point b, Point c, Color color)
      : FilledTriangle(a.x, a.y, b.x, b.y, c.x, c.y, color) {}

  FilledTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                 int16_t y2, Color color)
      : TriangleBase(x0, y0, x1, y1, x2, y2, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

}  // namespace roo_display
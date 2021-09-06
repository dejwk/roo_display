#pragma once

#include "roo_display.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

class BasicShape : virtual public Drawable {
 public:
  Color color() const { return color_; }

 protected:
  BasicShape(Color color) : color_(color) {}

 private:
  Color color_;
};

class Line : public BasicShape {
 public:
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

class Rect : public RectBase {
 public:
  Rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color)
      : RectBase(x0, y0, x1, y1, color) {}

 private:
  void drawTo(const Surface &s) const override;
};

class FilledRect : public RectBase, Rasterizable {
 public:
  FilledRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color)
      : RectBase(x0, y0, x1, y1, color) {}

  Box extents() const override { return RectBase::extents(); }

  TransparencyMode GetTransparencyMode() const override {
    return color().opaque() ? TRANSPARENCY_NONE : TRANSPARENCY_GRADUAL;
  }

  std::unique_ptr<PixelStream> CreateStream() const override;

  void ReadColors(const int16_t *x, const int16_t *y, uint32_t count,
                  Color *result) const override {
    Color::Fill(result, count, color());
  }

 private:
  void drawTo(const Surface &s) const override;
};

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

class RoundRect : public RoundRectBase {
 public:
  RoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t radius,
            Color color)
      : RoundRectBase(x0, y0, x1, y1, radius, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

class FilledRoundRect : public RoundRectBase {
 public:
  FilledRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  int16_t radius, Color color)
      : RoundRectBase(x0, y0, x1, y1, radius, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

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

class Circle : public CircleBase {
 public:
  static Circle ByRadius(int16_t x_center, int16_t y_center, int16_t radius,
                         Color color) {
    return Circle(x_center - radius, y_center - radius, (radius << 1) + 1,
                  color);
  }

  static Circle ByExtents(int16_t x0, int16_t y0, int16_t diameter,
                          Color color) {
    return Circle(x0, y0, diameter, color);
  }

 private:
  Circle(int16_t x0, int16_t y0, int16_t diameter, Color color)
      : CircleBase(x0, y0, diameter, color) {}

  void drawInteriorTo(const Surface &s) const override;
};

class FilledCircle : public CircleBase {
 public:
  static FilledCircle ByRadius(int16_t x_center, int16_t y_center,
                               int16_t radius, Color color) {
    return FilledCircle(x_center - radius, y_center - radius, (radius << 1) + 1,
                        color);
  }

  static FilledCircle ByExtents(int16_t x0, int16_t y0, int16_t diameter,
                                Color color) {
    return FilledCircle(x0, y0, diameter, color);
  }

 private:
  FilledCircle(int16_t x0, int16_t y0, int16_t diameter, Color color)
      : CircleBase(x0, y0, diameter, color) {}

  void drawInteriorTo(const Surface &s) const override;
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
  Triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
           int16_t y2, Color color)
      : TriangleBase(x0, y0, x1, y1, x2, y2, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

class FilledTriangle : public TriangleBase {
 public:
  FilledTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                 int16_t y2, Color color)
      : TriangleBase(x0, y0, x1, y1, x2, y2, color) {}

 private:
  void drawInteriorTo(const Surface &s) const override;
};

}  // namespace roo_display
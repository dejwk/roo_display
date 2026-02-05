#pragma once

#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/core/box.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/shape/point.h"
#include "roo_display/shape/smooth.h"

namespace roo_display {

/// Multi-point gradient specification.
class ColorGradient {
 public:
  /// A single node in the gradient.
  struct Node {
    float value;
    Color color;
  };

  /// Boundary behavior outside gradient range.
  enum Boundary {
    // Boundary values are extended to infinity.
    EXTENDED,

    // color::Transparent is assumed outside the boundary.
    TRUNCATED,

    // The gradient repeats periodically. The end boundary of the previous
    // repetition is the start boundary of a next repetition.
    PERIODIC,
  };

  /// Create a gradient specification.
  ///
  /// Node list must contain at least one node for EXTENDED/TRUNCATED, and at
  /// least two nodes with different values for PERIODIC. Nodes must be sorted
  /// by value. Equal successive values create sharp transitions.
  ColorGradient(std::vector<Node> gradient, Boundary boundary = EXTENDED);

  /// Return the color for a given value.
  ///
  /// Color is linearly interpolated between surrounding nodes. Values outside
  /// the range follow the boundary specification.
  Color getColor(float value) const;

  /// Return the transparency mode of the gradient.
  TransparencyMode getTransparencyMode() const { return transparency_mode_; }

 private:
  std::vector<Node> gradient_;
  Boundary boundary_;
  TransparencyMode transparency_mode_;
  float inv_period_;
};

/// Radial gradient based on distance from the center.
class RadialGradient : public Rasterizable {
 public:
  /// Create a radial gradient.
  ///
  /// Value equals distance from `center`. No anti-aliasing is performed; keep
  /// node distances >= 1 to avoid jagged rings. For smooth outer edges, add a
  /// terminal node with `color::Transparent` at +1 distance.
  RadialGradient(FpPoint center, ColorGradient gradient,
                 Box extents = Box::MaximumBox());

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

 private:
  float cx_;
  float cy_;
  ColorGradient gradient_;
  Box extents_;
};

/// Radial gradient using squared distance (faster, area-uniform).
class RadialGradientSq : public Rasterizable {
 public:
  /// Create a radial gradient using squared distance.
  ///
  /// Node values must be squared distances.
  RadialGradientSq(Point center, ColorGradient gradient,
                   Box extents = Box::MaximumBox());

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

 private:
  int16_t cx_;
  int16_t cy_;
  ColorGradient gradient_;
  Box extents_;
};

/// Linear gradient (horizontal, vertical, or skewed).
class LinearGradient : public Rasterizable {
 public:
  /// Create a linear gradient.
  ///
  /// Color value is computed as:
  /// \f$(x - origin.x) * dx + (y - origin.y) * dy\f$.
  /// Use `dx = 0` for vertical, `dy = 0` for horizontal gradients.
  /// No anti-aliasing is performed; for skewed gradients keep a minimum node
  /// spacing of \f$\sqrt{1/dx^2 + 1/dy^2}\f$.
  LinearGradient(Point origin, float dx, float dy, ColorGradient gradient,
                 Box extents = Box::MaximumBox());

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  int16_t cx_;
  int16_t cy_;
  float dx_;
  float dy_;
  ColorGradient gradient_;
  Box extents_;
};

/// Create a vertical gradient: \f$val = (x - x0) * dx\f$.
inline LinearGradient VerticalGradient(int16_t x0, float dx,
                                       ColorGradient gradient,
                                       Box extents = Box::MaximumBox()) {
  return LinearGradient({x0, 0}, dx, 0, gradient, extents);
}

/// Create a horizontal gradient: \f$val = (y - y0) * dy\f$.
inline LinearGradient HorizontalGradient(int16_t y0, float dy,
                                         ColorGradient gradient,
                                         Box extents = Box::MaximumBox()) {
  return LinearGradient({0, y0}, 0, dy, gradient, extents);
}

/// Angular gradient based on angle around `center`.
class AngularGradient : public Rasterizable {
 public:
  /// Create an angular gradient.
  ///
  /// Gradient values are interpreted from \f$-\pi\f$ (South) through
  /// \f$0\f$ (North) to \f$\pi\f$ (South). For other angle conventions, use a
  /// periodic gradient.
  AngularGradient(FpPoint center, ColorGradient gradient,
                  Box extents = Box::MaximumBox());

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

 private:
  float cx_;
  float cy_;
  ColorGradient gradient_;
  Box extents_;
};

}  // namespace roo_display

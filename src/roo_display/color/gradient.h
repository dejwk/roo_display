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

// General specification of a multi-point gradient.
class ColorGradient {
 public:
  // A single node in the gradient.
  struct Node {
    float value;
    Color color;
  };

  // Determines gradient behaves outside its bounds.
  enum Boundary {
    // Boundary values are extended to infinity.
    EXTENDED,

    // color::Transparent is assumed outside the boundary.
    TRUNCATED,

    // The gradient repeats periodically. The end boundary of the previous
    // repetition is the start boundary of a next repetition.
    PERIODIC,
  };

  // Creates a gradient specification, given the list of nodes and an option al
  // boundary specification. The node list must contain at least one node for
  // EXTENTED and TRUNCATED gradients, and at least two nodes with different
  // values for PERIODIC gradients. The nodes must be sorted by value. The
  // values in successive nodes can be equal; such condition indicates a sharp
  // color transition.
  ColorGradient(std::vector<Node> gradient, Boundary boundary = EXTENDED);

  // Returns a color corresponding to the specified numeric value. The color is
  // a linear interpolation of the colors of two nodes that the value is between
  // of. If the value is outside of the gradient range, the value is calculated
  // according to the boundary specification.
  Color getColor(float value) const;

  // Returns the transparency mode of the gradient. If all nodes are opaque, it
  // is TRANSPARENCY_NONE. Otherwise, if any node has a non-zero alpha, it is
  // TRANSPARENCY_GRADUAL. Otherwise, it is TRANSPARENCY_BINARY.
  TransparencyMode getTransparencyMode() const { return transparency_mode_; }

 private:
  std::vector<Node> gradient_;
  Boundary boundary_;
  TransparencyMode transparency_mode_;
  float inv_period_;
};

// Represents a radial gradient, in which the value depends solely on the
// distance of the target point from the gradient's center.
class RadialGradient : public Rasterizable {
 public:
  // Creates a radial gradient with the specified center, gradient
  // specification, and an optional bounding box. The value used to calculate
  // the gradient's color is the distance between the target point and the
  // `center`.
  //
  // This class does not perform anti-aliasing, so if you want to avoid jarred
  // circles, maintain distances between node point values to be at least 1. In
  // particular, if you want a smooth outer circle, add an artificial terminator
  // node with color::Transparent and the value 1 greater than its predecessor.
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

// Similar to RadialGradient, but uses the square of point distance to calculate
// the value. These gradients may sometimes look better, since the value covered
// by a given color is constant per area, rather than per distance. They also
// render faster, by avoiding the sqrtf calculation.
class RadialGradientSq : public Rasterizable {
 public:
  // Creates a radial gradient with the specified center, gradient
  // specification, and an optional bounding box. The value used to calculate
  // the gradient's color is the sqare of the distance between the target point
  // and the `center`. Consequently, the node values must be distances squared
  // as well.
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

// Represents an arbitrary linear gradient (horizontal, vertical, or skewed).
class LinearGradient : public Rasterizable {
 public:
  // Creates a linear gradient. The color value is calculated as:
  //
  // (x - origin.x) * dx + (y - origin.y) * dy
  //
  // Where (x, y) is the target point. Use dx = 0 for vertical gradients, and dy
  // = 0 for horizontal gradients.
  //
  // This class does not perform anti-aliasing, so if you use skewed gradients,
  // avoid sharp color transitions by maintaining a minimum distance of
  // sqrt(1/dx^2 + 1/dy^2) between the node values.
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

// Creates a vertical gradient, in which the color value depends solely on the x
// coordinate:
//
// val = (x - x0) * dx
inline LinearGradient VerticalGradient(int16_t x0, float dx,
                                       ColorGradient gradient,
                                       Box extents = Box::MaximumBox()) {
  return LinearGradient({x0, 0}, dx, 0, gradient, extents);
}

// Creates a horizontal gradient, in which the color value depends solely on the
// y coordinate:
//
// val = (y - y0) * dy
inline LinearGradient HorizontalGradient(int16_t y0, float dy,
                                         ColorGradient gradient,
                                         Box extents = Box::MaximumBox()) {
  return LinearGradient({0, y0}, 0, dy, gradient, extents);
}

// Represents an angular gradient, in which the color depends on an angle
// between the 'Y' axis (facing 'North') and a 'redius' line from the target
// point to the `center`.
class AngularGradient : public Rasterizable {
 public:
  // Creates the angular gradient, with the specified `center`, `gradient`
  // specification, and the optional extents.
  //
  // Gradient specification is assumed to go from -M_PI corresponding to
  // 'South', through -M_PI/2 corresponding to 'West', to 0 corresponding to
  // 'North', to M_PI/2 corresponding to 'East', to M_PI corresponding to
  // 'South' again. If your gradient spec is defined using different angle
  // boundaries, you may want to use a periodic specification.
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

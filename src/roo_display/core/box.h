#pragma once

#include <algorithm>

namespace roo_display {

/// Axis-aligned integer rectangle.
///
/// Coordinates are inclusive: width is `xMax - xMin + 1`.
class Box {
 public:
  /// Result of clipping a box to a clip region.
  enum ClipResult {
    CLIP_RESULT_EMPTY = 0,
    CLIP_RESULT_REDUCED,
    CLIP_RESULT_UNCHANGED
  };

  /// Return the intersection of two boxes (may be empty).
  inline static Box Intersect(const Box& a, const Box& b) {
    int16_t xMin = std::max(a.xMin(), b.xMin());
    int16_t yMin = std::max(a.yMin(), b.yMin());
    int16_t xMax = std::min(a.xMax(), b.xMax());
    int16_t yMax = std::min(a.yMax(), b.yMax());
    return Box(xMin, yMin, xMax, yMax);
  }

  /// Return the smallest box that contains both input boxes.
  inline static Box Extent(const Box& a, const Box& b) {
    int16_t xMin = std::min(a.xMin(), b.xMin());
    int16_t yMin = std::min(a.yMin(), b.yMin());
    int16_t xMax = std::max(a.xMax(), b.xMax());
    int16_t yMax = std::max(a.yMax(), b.yMax());
    return Box(xMin, yMin, xMax, yMax);
  }

  /// Construct a box from inclusive coordinates.
  ///
  /// If `xMax < xMin` or `yMax < yMin`, the box is made empty.
  constexpr Box(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax)
      : xMin_(xMin),
        yMin_(yMin),
        xMax_(xMax >= xMin ? xMax : xMin - 1),
        yMax_(yMax >= yMin ? yMax : yMin - 1) {}

  Box(const Box&) = default;
  Box(Box&&) = default;
  Box() = default;

  Box& operator=(const Box&) = default;
  Box& operator=(Box&&) = default;

  /// Return a large sentinel box used for unbounded extents.
  inline static Box MaximumBox() { return Box(-16384, -16384, 16383, 16383); }

  /// Return whether the box is empty.
  bool empty() const { return xMax_ < xMin_ || yMax_ < yMin_; }
  /// Minimum x (inclusive).
  int16_t xMin() const { return xMin_; }
  /// Minimum y (inclusive).
  int16_t yMin() const { return yMin_; }
  /// Maximum x (inclusive).
  int16_t xMax() const { return xMax_; }
  /// Maximum y (inclusive).
  int16_t yMax() const { return yMax_; }
  /// Width in pixels (inclusive coordinates).
  int16_t width() const { return xMax_ - xMin_ + 1; }
  /// Height in pixels (inclusive coordinates).
  int16_t height() const { return yMax_ - yMin_ + 1; }
  /// Area in pixels.
  int32_t area() const { return width() * height(); }

  /// Return whether the point (x, y) lies within the box.
  bool contains(int16_t x, int16_t y) const {
    return (x >= xMin_ && x <= xMax_ && y >= yMin_ && y <= yMax_);
  }

  /// Return whether this box fully contains the other box.
  bool contains(const Box& other) const {
    return (other.xMin() >= xMin_ && other.xMax() <= xMax_ &&
            other.yMin() >= yMin_ && other.yMax() <= yMax_);
  }

  /// Return whether this box intersects the other box.
  bool intersects(const Box& other) const {
    return other.xMin() <= xMax_ && other.xMax() >= xMin_ &&
           other.yMin() <= yMax_ && other.yMax() >= yMin_;
  }

  /// Clip this box to the given clip box.
  ///
  /// @return Clip result indicating whether the box was reduced or emptied.
  ClipResult clip(const Box& clip_box) {
    ClipResult result = CLIP_RESULT_UNCHANGED;
    if (xMin_ < clip_box.xMin()) {
      xMin_ = clip_box.xMin();
      result = CLIP_RESULT_REDUCED;
    }
    if (xMax_ > clip_box.xMax()) {
      xMax_ = clip_box.xMax();
      result = CLIP_RESULT_REDUCED;
    }
    if (yMin_ < clip_box.yMin()) {
      yMin_ = clip_box.yMin();
      result = CLIP_RESULT_REDUCED;
    }
    if (yMax_ > clip_box.yMax()) {
      yMax_ = clip_box.yMax();
      result = CLIP_RESULT_REDUCED;
    }
    return empty() ? CLIP_RESULT_EMPTY : result;
  }

  /// Return a translated copy of this box.
  Box translate(int16_t x_offset, int16_t y_offset) const {
    return Box(xMin_ + x_offset, yMin_ + y_offset, xMax_ + x_offset,
               yMax_ + y_offset);
  }

  /// Return a scaled copy of this box.
  Box scale(int16_t x_scale, int16_t y_scale) const {
    return Box(xMin_ * x_scale, yMin_ * y_scale, xMax_ * x_scale,
               yMax_ * y_scale);
  }

  /// Return a copy with x and y axes swapped.
  Box swapXY() const { return Box(yMin_, xMin_, yMax_, xMax_); }

  /// Return a copy mirrored across the Y-axis.
  Box flipX() const { return Box(-xMax_, yMin_, -xMin_, yMax_); }

  /// Return a copy mirrored across the X-axis.
  Box flipY() const { return Box(xMin_, -yMax_, xMax_, -yMin_); }

  /// Return a copy rotated 180 degrees around the origin.
  Box rotateUpsideDown() const { return Box(-xMax_, -yMax_, -xMin_, -yMin_); }

  /// Return a copy rotated 90 degrees clockwise around the origin.
  Box rotateRight() const { return Box(-yMax_, xMin_, -yMin_, xMax_); }

  /// Return a copy rotated 90 degrees counter-clockwise around the origin.
  Box rotateLeft() const { return Box(yMin_, -xMax_, yMax_, -xMin_); }

  /// Return the minimal box containing this box and the given point.
  Box extend(int16_t x, int16_t y) {
    int16_t x_min = std::min(xMin(), x);
    int16_t y_min = std::min(yMin(), y);
    int16_t x_max = std::max(xMax(), x);
    int16_t y_max = std::max(yMax(), y);
    return Box(x_min, y_min, x_max, y_max);
  }

 private:
  int16_t xMin_, yMin_, xMax_, yMax_;
};

/// Equality operator for boxes.
inline bool operator==(const Box& a, const Box& b) {
  return a.xMin() == b.xMin() && a.yMin() == b.yMin() && a.xMax() == b.xMax() &&
         a.yMax() == b.yMax();
}

/// Inequality operator for boxes.
inline bool operator!=(const Box& a, const Box& b) { return !(a == b); }

}  // namespace roo_display

#if defined(__linux__) || defined(__linux) || defined(linux)
#include <ostream>

namespace roo_display {
inline std::ostream& operator<<(std::ostream& os, const roo_display::Box& box) {
  os << "[" << box.xMin() << ", " << box.yMin() << ", " << box.xMax() << ", "
     << box.yMax() << "]";
  return os;
}
}  // namespace roo_display
#endif  // defined(__linux__)

#pragma once

#include <algorithm>

namespace roo_display {

class Box {
 public:
  enum ClipResult {
    CLIP_RESULT_EMPTY = 0,
    CLIP_RESULT_REDUCED,
    CLIP_RESULT_UNCHANGED
  };

  inline static Box intersect(const Box& a, const Box& b) {
    int16_t xMin = std::max(a.xMin(), b.xMin());
    int16_t yMin = std::max(a.yMin(), b.yMin());
    int16_t xMax = std::min(a.xMax(), b.xMax());
    int16_t yMax = std::min(a.yMax(), b.yMax());
    return Box(xMin, yMin, xMax, yMax);
  }

  inline static Box extent(const Box& a, const Box& b) {
    int16_t xMin = std::min(a.xMin(), b.xMin());
    int16_t yMin = std::min(a.yMin(), b.yMin());
    int16_t xMax = std::max(a.xMax(), b.xMax());
    int16_t yMax = std::max(a.yMax(), b.yMax());
    return Box(xMin, yMin, xMax, yMax);
  }

  Box(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax)
      : xMin_(xMin), yMin_(yMin), xMax_(xMax), yMax_(yMax) {
    if (xMax_ < xMin_) xMax_ = xMin_ - 1;
    if (yMax_ < yMin_) yMax_ = yMin_ - 1;
  }

  Box(const Box&) = default;
  Box(Box&&) = default;
  Box() = default;

  Box& operator=(const Box&) = default;
  Box& operator=(Box&&) = default;

  inline static Box MaximumBox() { return Box(-16384, -16384, 16383, 16383); }

  bool empty() const { return xMax_ < xMin_ || yMax_ < yMin_; }
  int16_t xMin() const { return xMin_; }
  int16_t yMin() const { return yMin_; }
  int16_t xMax() const { return xMax_; }
  int16_t yMax() const { return yMax_; }
  int16_t width() const { return xMax_ - xMin_ + 1; }
  int16_t height() const { return yMax_ - yMin_ + 1; }
  int32_t area() const { return width() * height(); }

  bool contains(int16_t x, int16_t y) const {
    return (x >= xMin_ && x <= xMax_ && y >= yMin_ && y <= yMax_);
  }

  bool contains(const Box& other) const {
    return (other.xMin() >= xMin_ && other.xMax() <= xMax_ &&
            other.yMin() >= yMin_ && other.yMax() <= yMax_);
  }

  bool intersects(const Box& other) const {
    return other.xMin() <= xMax_ && other.xMax() >= xMin_ &&
           other.yMin() <= yMax_ && other.yMax() >= yMin_;
  }

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

  Box translate(int16_t x_offset, int16_t y_offset) const {
    return Box(xMin_ + x_offset, yMin_ + y_offset, xMax_ + x_offset,
               yMax_ + y_offset);
  }

  Box scale(int16_t x_scale, int16_t y_scale) const {
    return Box(xMin_ * x_scale, yMin_ * y_scale, xMax_ * x_scale,
               yMax_ * y_scale);
  }

  Box swapXY() const { return Box(yMin_, xMin_, yMax_, xMax_); }

  Box flipX() const { return Box(-xMax_, yMin_, -xMin_, yMax_); }

  Box flipY() const { return Box(xMin_, -yMax_, xMax_, -yMin_); }

  Box rotateUpsideDown() const { return Box(-xMax_, -yMax_, -xMin_, -yMin_); }

  Box rotateRight() const { return Box(-yMax_, xMin_, -yMin_, xMax_); }

  Box rotateLeft() const { return Box(yMin_, -xMax_, yMax_, -xMin_); }

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

inline bool operator==(const Box& a, const Box& b) {
  return a.xMin() == b.xMin() && a.yMin() == b.yMin() && a.xMax() == b.xMax() &&
         a.yMax() == b.yMax();
}

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
}
#endif  // defined(__linux__)

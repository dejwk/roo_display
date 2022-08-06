#pragma once

#include "roo_display/core/box.h"

namespace roo_display {

enum Anchor {
  ANCHOR_ORIGIN = 0,  // Point with the zero coordinate.
  ANCHOR_MIN = 1,     // Left or top.
  ANCHOR_MID = 2,     // Center or middle.
  ANCHOR_MAX = 3      // Right or bottom.
};

namespace internal {

inline int16_t resolveAnchor(Anchor anchor, int16_t first, int16_t last) {
  switch (anchor) {
    case ANCHOR_MIN:
      return first;
    case ANCHOR_MID:
      return (first + last) / 2;
    case ANCHOR_MAX:
      return last;
    case ANCHOR_ORIGIN:
    default:
      return 0;
  }
}

class AlignBase {
 public:
  explicit constexpr AlignBase() : AlignBase(ANCHOR_ORIGIN, ANCHOR_ORIGIN, 0) {}

  explicit constexpr AlignBase(Anchor dst, Anchor src, int16_t shift)
      : rep_((uint16_t)(shift + (1 << 11)) << 4 | src << 2 | dst) {}

  constexpr Anchor src() const { return (Anchor)((rep_ >> 2) & 3); }
  constexpr Anchor dst() const { return (Anchor)(rep_ & 3); }

  constexpr int16_t shift() const { return (int16_t)((rep_ >> 4) - (1 << 11)); }

  int16_t resolveOffset(int16_t first_outer, int16_t last_outer,
                        int16_t first_inner, int16_t last_inner) const {
    return resolveAnchor(dst(), first_outer, last_outer) -
           resolveAnchor(src(), first_inner, last_inner) + shift();
  }

  uint16_t rep_;
};

}  // namespace internal

// Represents a horizontal alignment. Consists of three components:
// 1. the 'source' anchor, determining which coordinate of the source should get
// aligned (left, center, right, or the origin)
// 2. the 'destination' anchor, determining to which coordinate of the
// destination should the source be aligned (like above)
// 3. an additional absolute offset (padding).
//
// You generally shouldn't construct these directly. Instead, start with one
// of the constants and customize it, e.g.:
//
// kLeft.toCenter().shiftBy(5) - aligns the left boundary of the source to be
// 5 pixels to the right of the center of the destination.
class HAlign : public internal::AlignBase {
 public:
  using AlignBase::AlignBase;

  // Applies an extra horizontal shift.
  constexpr HAlign shiftBy(int16_t shift_by) {
    return HAlign(dst(), src(), shift() + shift_by);
  }

  // Sets the anchor relative to the left of the destination.
  constexpr HAlign toLeft() { return HAlign(ANCHOR_MIN, src(), shift()); }

  // Sets the anchor relative to the center of the destination.
  constexpr HAlign toCenter() { return HAlign(ANCHOR_MID, src(), shift()); }

  // Sets the anchor relative to the right of the destination.
  constexpr HAlign toRight() { return HAlign(ANCHOR_MAX, src(), shift()); }

  // Sets the anchor relative to the origin (point zero) of the destination.
  constexpr HAlign toOrigin() { return HAlign(ANCHOR_ORIGIN, src(), shift()); }
};

// Left-to-left with no shift.
static constexpr HAlign kLeft = HAlign(ANCHOR_MIN, ANCHOR_MIN, 0);

// Center-to-center with no shirt.
static constexpr HAlign kCenter = HAlign(ANCHOR_MID, ANCHOR_MID, 0);

// Right-to-right with no shift.
static constexpr HAlign kRight = HAlign(ANCHOR_MAX, ANCHOR_MAX, 0);

// Origin-to-origin with no shift. (I.e. pretty much no alignment at all).
static constexpr HAlign kOrigin = HAlign(ANCHOR_ORIGIN, ANCHOR_ORIGIN, 0);

// Represents a vertical alignment. Consists of three components:
// 1. the 'source' anchor, determining which coordinate of the source should get
// aligned (top, middle, bottom, or the baseline = coordinate zero)
// 2. the 'destination' anchor, determining to which coordinate of the
// destination should the source be aligned (like above)
// 3. an additional absolute offset (padding).
//
// You generally shouldn't construct these directly. Instead, start with one
// of the constants and customize it, e.g.:
//
// kTop.toMiddle().shiftBy(5) - aligns the top boundary of the source to be
// 5 pixels to the bottom of the middle of the destination.
class VAlign : public internal::AlignBase {
 public:
  using AlignBase::AlignBase;

  constexpr VAlign shiftBy(int16_t shift_by) {
    return VAlign(dst(), src(), shift() + shift_by);
  }

  // Sets the anchor relative to the top of the destination.
  constexpr VAlign toTop() { return VAlign(ANCHOR_MIN, src(), shift()); }

  // Sets the anchor relative to the middle of the destination.
  constexpr VAlign toMiddle() { return VAlign(ANCHOR_MID, src(), shift()); }

  // Sets the anchor relative to the bottom of the destination.
  constexpr VAlign toBottom() { return VAlign(ANCHOR_MAX, src(), shift()); }

  // Sets the anchor relative to the baseline (zero coordinate) of the
  // destination.
  constexpr VAlign toBaseline() {
    return VAlign(ANCHOR_ORIGIN, src(), shift());
  }
};

// Top-to-top with no shift.
static constexpr VAlign kTop = VAlign(ANCHOR_MIN, ANCHOR_MIN, 0);

// Middle-to-middle with no shift.
static constexpr VAlign kMiddle = VAlign(ANCHOR_MID, ANCHOR_MID, 0);

// Bottom-to-bottom with no shift.
static constexpr VAlign kBottom = VAlign(ANCHOR_MAX, ANCHOR_MAX, 0);

// Baseline-to-baseline with no shift. (I.e., pretty much no alignment).
static constexpr VAlign kBaseline = VAlign(ANCHOR_ORIGIN, ANCHOR_ORIGIN, 0);

// Combines the horizontal and vertical alignments. Lightweight enough to
// be passed by value.
// Can be constructed using the '|' operator, e.g.: kTop.shift(5) | kMiddle.
class Alignment {
 public:
  constexpr Alignment() : h_(), v_() {}

  constexpr Alignment(HAlign h) : h_(h), v_() {}

  constexpr Alignment(VAlign v) : h_(), v_(v) {}

  constexpr Alignment(HAlign h, VAlign v) : h_(h), v_(v) {}

  constexpr HAlign h() const { return h_; }

  constexpr VAlign v() const { return v_; }

  std::pair<int16_t, int16_t> resolveOffset(const Box& outer,
                                            const Box& inner) const {
    return std::make_pair(h().resolveOffset(outer.xMin(), outer.xMax(),
                                            inner.xMin(), inner.xMax()),
                          v().resolveOffset(outer.yMin(), outer.yMax(),
                                            inner.yMin(), inner.yMax()));
  }

 private:
  HAlign h_;
  VAlign v_;
};

// Absolute alignment - the content is not repositioned either
// horizontally or vertically.
static constexpr Alignment kNoAlign = Alignment();

// Combines the horizontal and vertical alignments.
inline constexpr Alignment operator|(HAlign h, VAlign v) {
  return Alignment(h, v);
}

// Combines the horizontal and vertical alignments.
inline constexpr Alignment operator|(VAlign v, HAlign h) {
  return Alignment(h, v);
}

}  // namespace roo_display
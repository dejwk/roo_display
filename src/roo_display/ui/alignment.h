#pragma once

#include <inttypes.h>

#include "roo_display/core/box.h"

namespace roo_display {

/// Anchor point used for alignment.
enum Anchor {
  ANCHOR_ORIGIN = 0,  // Point with the zero coordinate.
  ANCHOR_MIN = 1,     // Left or top.
  ANCHOR_MID = 2,     // Center or middle.
  ANCHOR_MAX = 3      // Right or bottom.
};

namespace internal {

template <typename Dim>
inline Dim resolveAnchor(Anchor anchor, Dim first, Dim last) {
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

  template <typename Dim>
  Dim resolveOffset(Dim first_outer, Dim last_outer, Dim first_inner,
                    Dim last_inner) const {
    return resolveAnchor<Dim>(dst(), first_outer, last_outer) -
           resolveAnchor<Dim>(src(), first_inner, last_inner) + shift();
  }

  bool operator==(AlignBase other) const { return rep_ == other.rep_; }

  bool operator!=(AlignBase other) const { return rep_ != other.rep_; }

 protected:
  uint16_t rep_;
};

}  // namespace internal

/// Horizontal alignment.
///
/// Consists of:
/// 1) source anchor (left/center/right/origin)
/// 2) destination anchor (left/center/right/origin)
/// 3) absolute offset (padding)
///
/// Prefer using the constants and modifying them, e.g.
/// `kLeft.toCenter().shiftBy(5)` aligns the left boundary of the source
/// to be 5 pixels to the right of the destination center.
class HAlign : public internal::AlignBase {
 public:
  using AlignBase::AlignBase;

  // Applies an extra horizontal shift.
  constexpr HAlign shiftBy(int16_t shift_by) const {
    return HAlign(dst(), src(), shift() + shift_by);
  }

  // Sets the anchor relative to the left of the destination.
  constexpr HAlign toLeft() const { return HAlign(ANCHOR_MIN, src(), shift()); }

  // Sets the anchor relative to the center of the destination.
  constexpr HAlign toCenter() const {
    return HAlign(ANCHOR_MID, src(), shift());
  }

  // Sets the anchor relative to the right of the destination.
  constexpr HAlign toRight() const {
    return HAlign(ANCHOR_MAX, src(), shift());
  }

  // Sets the anchor relative to the origin (point zero) of the destination.
  constexpr HAlign toOrigin() const {
    return HAlign(ANCHOR_ORIGIN, src(), shift());
  }
};

/// Left-to-left with no shift.
static constexpr HAlign kLeft = HAlign(ANCHOR_MIN, ANCHOR_MIN, 0);

/// Center-to-center with no shift.
static constexpr HAlign kCenter = HAlign(ANCHOR_MID, ANCHOR_MID, 0);

/// Right-to-right with no shift.
static constexpr HAlign kRight = HAlign(ANCHOR_MAX, ANCHOR_MAX, 0);

/// Origin-to-origin with no shift.
static constexpr HAlign kOrigin = HAlign(ANCHOR_ORIGIN, ANCHOR_ORIGIN, 0);

/// Vertical alignment.
///
/// Consists of:
/// 1) source anchor (top/middle/bottom/baseline)
/// 2) destination anchor (top/middle/bottom/baseline)
/// 3) absolute offset (padding)
///
/// Prefer using the constants and modifying them, e.g.
/// `kTop.toMiddle().shiftBy(5)` aligns the top boundary of the source
/// to be 5 pixels below the destination middle.
class VAlign : public internal::AlignBase {
 public:
  using AlignBase::AlignBase;

  constexpr VAlign shiftBy(int16_t shift_by) const {
    return VAlign(dst(), src(), shift() + shift_by);
  }

  // Sets the anchor relative to the top of the destination.
  constexpr VAlign toTop() const { return VAlign(ANCHOR_MIN, src(), shift()); }

  // Sets the anchor relative to the middle of the destination.
  constexpr VAlign toMiddle() const {
    return VAlign(ANCHOR_MID, src(), shift());
  }

  // Sets the anchor relative to the bottom of the destination.
  constexpr VAlign toBottom() const {
    return VAlign(ANCHOR_MAX, src(), shift());
  }

  // Sets the anchor relative to the baseline (zero coordinate) of the
  // destination.
  constexpr VAlign toBaseline() const {
    return VAlign(ANCHOR_ORIGIN, src(), shift());
  }
};

/// Top-to-top with no shift.
static constexpr VAlign kTop = VAlign(ANCHOR_MIN, ANCHOR_MIN, 0);

/// Middle-to-middle with no shift.
static constexpr VAlign kMiddle = VAlign(ANCHOR_MID, ANCHOR_MID, 0);

/// Bottom-to-bottom with no shift.
static constexpr VAlign kBottom = VAlign(ANCHOR_MAX, ANCHOR_MAX, 0);

/// Baseline-to-baseline with no shift.
static constexpr VAlign kBaseline = VAlign(ANCHOR_ORIGIN, ANCHOR_ORIGIN, 0);

struct Offset {
  int16_t dx;
  int16_t dy;
};

/// Combines horizontal and vertical alignment.
///
/// Lightweight and pass-by-value. Use `|` to compose, e.g.
/// `kTop.shiftBy(5) | kMiddle`.
class Alignment {
 public:
  constexpr Alignment() : h_(), v_() {}

  constexpr Alignment(HAlign h) : h_(h), v_() {}

  constexpr Alignment(VAlign v) : h_(), v_(v) {}

  constexpr Alignment(HAlign h, VAlign v) : h_(h), v_(v) {}

  constexpr HAlign h() const { return h_; }

  constexpr VAlign v() const { return v_; }

  Offset resolveOffset(const Box& outer, const Box& inner) const {
    return Offset{.dx = h().resolveOffset(outer.xMin(), outer.xMax(),
                                          inner.xMin(), inner.xMax()),
                  .dy = v().resolveOffset(outer.yMin(), outer.yMax(),
                                          inner.yMin(), inner.yMax())};
  }

  Alignment shiftBy(int16_t dx, int16_t dy) {
    return Alignment(h_.shiftBy(dx), v_.shiftBy(dy));
  }

  bool operator==(Alignment other) const {
    return h_ == other.h_ && v_ == other.v_;
  }

  bool operator!=(Alignment other) const {
    return h_ != other.h_ || v_ != other.v_;
  }

 private:
  HAlign h_;
  VAlign v_;
};

/// Absolute alignment (no repositioning).
static constexpr Alignment kNoAlign = Alignment();

/// Combine horizontal and vertical alignments.
inline constexpr Alignment operator|(HAlign h, VAlign v) {
  return Alignment(h, v);
}

/// Combine vertical and horizontal alignments.
inline constexpr Alignment operator|(VAlign v, HAlign h) {
  return Alignment(h, v);
}

}  // namespace roo_display
#pragma once

#include "roo_display/core/box.h"

namespace roo_display {

namespace internal {

enum Anchor { NONE = 0, MIN = 1, MID = 2, MAX = 3 };

class AlignBase {
 protected:
  friend class OffsetAlignBase;

  explicit constexpr AlignBase(Anchor anchor) : anchor_(anchor) {}

  int16_t GetOffset(int16_t first_outer, int16_t last_outer,
                    int16_t first_inner, int16_t last_inner) const {
    switch (anchor_) {
      case MIN:
        return first_outer - first_inner;
      case MID:
        return first_outer - first_inner +
               (last_outer - first_outer + first_inner - last_inner) / 2;
      case MAX:
        return last_outer - last_inner;
      case NONE:
      default:
        return 0;
    }
  }

  Anchor anchor_;
};

class OffsetAlignBase {
 protected:
  explicit constexpr OffsetAlignBase() : OffsetAlignBase(NONE, 0) {}

  explicit constexpr OffsetAlignBase(Anchor anchor, int16_t offset)
      : rep_((uint16_t)offset << 2 | anchor) {}

  constexpr Anchor my_anchor() const { return (Anchor)(rep_ & 3); }

  constexpr int16_t my_offset() const { return (int16_t)(rep_ >> 2); }

  int16_t GetOffset(int16_t first_outer, int16_t last_outer,
                    int16_t first_inner, int16_t last_inner) const {
    return AlignBase(my_anchor())
               .GetOffset(first_outer, last_outer, first_inner, last_inner) +
           my_offset();
  }

  uint16_t rep_;
};

}  // namespace internal

class OffsetHAlign : public internal::OffsetAlignBase {
 public:
  OffsetHAlign adjustedBy(int16_t offset) {
    return OffsetHAlign(my_anchor(), my_offset() + offset);
  }

  // Returns an absolute horizontal pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  // The result is then used to horizontally shift the interior.
  int16_t GetOffset(const Box &extents, const Box &interior) const {
    return internal::OffsetAlignBase::GetOffset(
        extents.xMin(), extents.xMax(), interior.xMin(), interior.xMax());
  }

  // Returns an absolute horizontal pixel offset implied by this alignment
  // object, for the specified xMin and xMax.
  int16_t GetOffset(int16_t xMin, int16_t xMax) const {
    return internal::OffsetAlignBase::GetOffset(xMin, xMax, 0, 0);
  }

 private:
  friend class HAlign;

  using OffsetAlignBase::OffsetAlignBase;
};

// Represents horizontal alignment with respect to a bounding rectangle.
class HAlign : public internal::AlignBase {
 public:
  explicit constexpr HAlign(internal::Anchor anchor)
      : internal::AlignBase(anchor) {}

  constexpr OffsetHAlign adjustedBy(int16_t offset) {
    return OffsetHAlign(anchor_, offset);
  }

  // Returns an absolute horizontal pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  // The result is then used to horizontally shift the interior.
  int16_t GetOffset(const Box &extents, const Box &interior) const {
    return internal::AlignBase::GetOffset(extents.xMin(), extents.xMax(),
                                          interior.xMin(), interior.xMax());
  }

  // Returns an absolute horizontal pixel offset implied by this alignment
  // object, for the specified xMin and xMax.
  int16_t GetOffset(int16_t xMin, int16_t xMax) const {
    return internal::AlignBase::GetOffset(xMin, xMax, 0, 0);
  }

 private:
  friend class Alignment;
};

// An alignment that will horitontally align an interior rectangle to
// the left of a bounding rectangle.
static constexpr HAlign kLeft = HAlign(internal::MIN);

// An alignment that will horitontally align an interior rectangle at
// the center of a bounding rectangle.
static constexpr HAlign kCenter = HAlign(internal::MID);

// An alignment that will horitontally align an interior rectangle to
// the right of a bounding rectangle.
static constexpr HAlign kRight = HAlign(internal::MAX);

// A nil-alignment that will use the interior rectangle's absolute
// horizontal extents.
static constexpr HAlign kNoHAlign = HAlign(internal::NONE);

class OffsetVAlign : public internal::OffsetAlignBase {
 public:
  OffsetVAlign offset(int16_t offset) {
    return OffsetVAlign(my_anchor(), my_offset() + offset);
  }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  // The result is then used to vertically shift the interior.
  int16_t GetOffset(const Box &outer, const Box &inner) const {
    return internal::OffsetAlignBase::GetOffset(outer.yMin(), outer.yMax(),
                                                inner.yMin(), inner.yMax());
  }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified vertical bounds and the interior.
  int16_t GetOffset(int16_t yMinOuter, int16_t yMaxOuter, int16_t yMinInner,
                    int16_t yMaxInner) const {
    return internal::OffsetAlignBase::GetOffset(yMinOuter, yMaxOuter, yMinInner,
                                                yMaxInner);
  }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  int16_t GetOffset(int16_t yMin, int16_t yMax) const {
    return internal::OffsetAlignBase::GetOffset(yMin, yMax, 0, 0);
  }

 private:
  friend class VAlign;

  using OffsetAlignBase::OffsetAlignBase;
};

// Represents vertical alignment with respect to a bounding rectangle.
class VAlign : public internal::AlignBase {
 public:
  explicit constexpr VAlign(internal::Anchor anchor)
      : internal::AlignBase(anchor) {}

  constexpr OffsetVAlign adjustedBy(int16_t offset) {
    return OffsetVAlign(anchor_, offset);
  }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  // The result is then used to vertically shift the interior.
  int16_t GetOffset(const Box &outer, const Box &inner) const {
    return internal::AlignBase::GetOffset(outer.yMin(), outer.yMax(),
                                          inner.yMin(), inner.yMax());
  }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified vertical bounds and the interior.
  int16_t GetOffset(int16_t yMinOuter, int16_t yMaxOuter, int16_t yMinInner,
                    int16_t yMaxInner) const {
    return internal::AlignBase::GetOffset(yMinOuter, yMaxOuter, yMinInner,
                                          yMaxInner);
  }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  int16_t GetOffset(int16_t yMin, int16_t yMax) const {
    return internal::AlignBase::GetOffset(yMin, yMax, 0, 0);
  }

 private:
  friend class Alignment;
};

// An alignment that will vertically align an interior rectangle to
// the top of a bounding rectangle.
static constexpr VAlign kTop = VAlign(internal::MIN);

// An alignment that will vertically align an interior rectangle in
// the middle of a bounding rectangle.
static constexpr VAlign kMiddle = VAlign(internal::MID);

// An alignment that will vertically align an interior rectangle to
// the bottom of a bounding rectangle.
static constexpr VAlign kBottom = VAlign(internal::MAX);

// A nil-alignment that will use the interior rectangle's absolute
// vertical extents, with an optional additional absolute pixel offset.
static constexpr VAlign kNoVAlign = VAlign(internal::NONE);

class Alignment {
 public:
  constexpr Alignment() : h_(), v_() {}

  constexpr Alignment(HAlign h) : h_(h.adjustedBy(0)), v_() {}

  constexpr Alignment(OffsetHAlign h) : h_(h), v_() {}

  constexpr Alignment(VAlign v) : h_(), v_(v.adjustedBy(0)) {}

  constexpr Alignment(OffsetVAlign v) : h_(), v_(v) {}

  constexpr Alignment(OffsetHAlign h, OffsetVAlign v) : h_(h), v_(v) {}

  constexpr OffsetHAlign h() const { return h_; }

  constexpr OffsetVAlign v() const { return v_; }

 private:
  OffsetHAlign h_;
  OffsetVAlign v_;
};

// Absolute alignment - the content is not repositioned either
// horizontally or vertically.
static constexpr Alignment kNoAlign = Alignment();

// inline constexpr Align operator+(HAlign h, VAlign v) {
//   return Align(h, v);
// }

// inline constexpr Align operator+(VAlign v, HAlign h) {
//   return Align(h, v);
// }

inline constexpr Alignment operator|(HAlign h, VAlign v) {
  return Alignment(h.adjustedBy(0), v.adjustedBy(0));
}

inline constexpr Alignment operator|(HAlign h, OffsetVAlign v) {
  return Alignment(h.adjustedBy(0), v);
}

inline constexpr Alignment operator|(OffsetHAlign h, VAlign v) {
  return Alignment(h, v.adjustedBy(0));
}

inline constexpr Alignment operator|(OffsetHAlign h, OffsetVAlign v) {
  return Alignment(h, v);
}

inline constexpr Alignment operator|(VAlign v, HAlign h) {
  return Alignment(h.adjustedBy(0), v.adjustedBy(0));
}

inline constexpr Alignment operator|(VAlign v, OffsetHAlign h) {
  return Alignment(h, v.adjustedBy(0));
}

inline constexpr Alignment operator|(OffsetVAlign v, HAlign h) {
  return Alignment(h.adjustedBy(0), v);
}

inline constexpr Alignment operator|(OffsetVAlign v, OffsetHAlign h) {
  return Alignment(h, v);
}

}  // namespace roo_display
#pragma once

#include "roo_display/core/box.h"

namespace roo_display {

namespace internal {

class Align {
 public:
  enum Anchor { MIN, MID, MAX, NONE };

 protected:
  Align(Anchor anchor, int16_t offset) : anchor_(anchor), offset_(offset) {}

  int16_t GetOffset(int16_t first_outer, int16_t last_outer,
                    int16_t first_inner, int16_t last_inner) const {
    switch (anchor_) {
      case MIN:
        return first_outer - first_inner + offset_;
      case MID:
        return first_outer - first_inner + offset_ +
               (last_outer - first_outer + first_inner - last_inner) / 2;
      case MAX:
        return offset_ + last_outer - last_inner;
      case NONE:
        return offset_;
    }
  }

 private:
  Anchor anchor_;
  int16_t offset_;
};

}  // namespace internal

// Represents horizontal alignment with respect to a bounding rectangle.
class HAlign : public internal::Align {
 public:
  // Creates an alignment that will horitontally align an interior rectangle to
  // the left of a bounding rectangle, with an optional additional absolute
  // pixel offset.
  static HAlign Left(int16_t offset = 0) { return HAlign(Align::MIN, offset); }

  // Creates an alignment that will horitontally align an interior rectangle at
  // the center of a bounding rectangle, with an optional additional absolute
  // pixel offset.
  static HAlign Center(int16_t offset = 0) {
    return HAlign(Align::MID, offset);
  }

  // Creates an alignment that will horitontally align an interior rectangle to
  // the right of a bounding rectangle, with an optional additional absolute
  // pixel offset. (Use negative offset to create extra border between the
  // interior and the right bound).
  static HAlign Right(int16_t offset = 0) { return HAlign(Align::MAX, offset); }

  // Creates a nil-alignment that will use the interior rectangle's absolute
  // horizontal extents, with an optional additional absolute pixel offset.
  static HAlign None(int16_t offset = 0) { return HAlign(Align::NONE, offset); }

  // Returns an absolute horizontal pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  // The result is then used to horizontally shift the interior.
  int16_t GetOffset(const Box &extents, const Box &interior) const {
    return internal::Align::GetOffset(extents.xMin(), extents.xMax(),
                                      interior.xMin(), interior.xMax());
  }

 private:
  HAlign(Align::Anchor anchor, int16_t offset) : Align(anchor, offset) {}
};

// Represents vertical alignment with respect to a bounding rectangle.
class VAlign : public internal::Align {
 public:
  // Creates an alignment that Will vertically align an interior rectangle to
  // the top of a bounding rectangle, with an optional additional absolute
  // pixel offset.
  static VAlign Top(int16_t offset = 0) { return VAlign(Align::MIN, offset); }

  // Creates an alignment that will vertically align an interior rectangle in
  // the middle of a bounding rectangle, with an optional additional absolute
  // pixel offset.
  static VAlign Middle(int16_t offset = 0) {
    return VAlign(Align::MID, offset);
  }

  // Creates an alignment that will vertically align an interior rectangle to
  // the bottom of a bounding rectangle, with an optional additional absolute
  // pixel offset. (Use negative offset to create extra border between the
  // interior and the bottom bound).
  static VAlign Bottom(int16_t offset = 0) {
    return VAlign(Align::MAX, offset);
  }

  // Creates a nil-alignment that will use the interior rectangle's absolute
  // vertical extents, with an optional additional absolute pixel offset.
  static VAlign None(int16_t offset = 0) { return VAlign(Align::NONE, offset); }

  // Returns an absolute vertical pixel offset implied by this alignment
  // object, for the specified bounding rectangle and the interior rectangle.
  // The result is then used to vertically shift the interior.
  int16_t GetOffset(const Box &outer, const Box &inner) const {
    return internal::Align::GetOffset(outer.yMin(), outer.yMax(), inner.yMin(),
                                      inner.yMax());
  }

 private:
  VAlign(Align::Anchor anchor, int16_t offset) : Align(anchor, offset) {}
};

}  // namespace roo_display
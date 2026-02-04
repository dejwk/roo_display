#pragma once

#include "roo_display/color/color.h"
#include "roo_display/core/box.h"
#include "roo_display/core/drawable.h"
#include "roo_display/filter/background.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

namespace internal {

class SolidBorder {
 public:
  SolidBorder(Box extents, Box interior, Box anchorExtents, Alignment alignment)
      : extents_(std::move(extents)),
        offset_(alignment.resolveOffset(extents_, anchorExtents)),
        interior_(Box::Intersect(interior.translate(offset_.dx, offset_.dy),
                                 extents_)) {}

  const Box &extents() const { return extents_; }
  int16_t x_offset() const { return offset_.dx; }
  int16_t y_offset() const { return offset_.dy; }
  const Box &interior() const { return interior_; }

 private:
  // Absolute extents, when the border is drawn at (0, 0).
  Box extents_;

  // Absolute offset by which the interior needs to be shifted in order to
  // comply with requested alignment.
  Offset offset_;

  // Absolute coordinates of the (aligned) interior, when the border is drawn at
  // (0, 0), truncated to the extents_.
  Box interior_;
};

class TileBase : public Drawable {
 public:
  TileBase(const Drawable &interior, Box extents, Alignment alignment,
           Color bgcolor = color::Background)
      : border_(std::move(extents), interior.extents(),
                interior.anchorExtents(), alignment),
        bgcolor_(bgcolor),
        background_(nullptr) {}

  void setBgColor(Color bgcolor) {
    bgcolor_ = bgcolor;
    background_ = nullptr;
  }

  void setBackground(const Rasterizable *background) {
    bgcolor_ = color::Background;
    background_ = background;
  }

  Box extents() const override { return border_.extents(); }

 protected:
  void draw(const Surface &s, const Drawable &interior) const;
  void drawInternal(const Surface &s, const Drawable &interior) const;

 private:
  internal::SolidBorder border_;
  Color bgcolor_;
  const Rasterizable *background_;
};

}  // namespace internal

/// Rectangular drawable with background and aligned interior.
///
/// Useful for UI widgets (icons, text boxes, etc.). Handles alignment and
/// flicker-minimized redraws.
///
/// Alignment: the interior is positioned inside the tile using the alignment
/// specification (horizontal and vertical anchors plus optional offsets).
///
/// Flickerless redraws: the tile is drawn as up to 5 non-overlapping sections
/// (top, left, interior, right, bottom) to avoid double draws.
///
/// Semi-transparent backgrounds are supported. The tile background is
/// alpha-blended over the draw-time background color.
class Tile : public internal::TileBase {
 public:
  /// Create a tile with the specified interior, extents, and alignment.
  Tile(const Drawable *interior, Box extents, Alignment alignment,
       Color bgcolor = color::Background)
      : internal::TileBase(*interior, extents, alignment, bgcolor),
        interior_(interior) {}

  void drawTo(const Surface &s) const override {
    TileBase::draw(s, *interior_);
  }

 private:
  const Drawable *interior_;
};

/// Tile with embedded interior object.
///
/// Useful when returning a tile without keeping a separate interior object.
template <typename DrawableType>
class TileOf : public internal::TileBase {
 public:
  /// Create a tile with embedded interior and alignment.
  TileOf(DrawableType interior, Box extents, Alignment alignment,
         Color bgcolor = color::Background)
      : internal::TileBase(interior, extents, alignment, bgcolor),
        interior_(std::move(interior)) {}

  void drawTo(const Surface &s) const override { TileBase::draw(s, interior_); }

 private:
  DrawableType interior_;
};

/// Convenience factory for a `TileOf`.
template <typename DrawableType>
TileOf<DrawableType> MakeTileOf(DrawableType interior, Box extents,
                                Alignment alignment = kNoAlign,
                                Color bgcolor = color::Background) {
  return TileOf<DrawableType>(std::move(interior), std::move(extents),
                              alignment, bgcolor);
}

}  // namespace roo_display
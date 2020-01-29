#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

namespace internal {

class SolidBorder {
 public:
  SolidBorder(Box extents, Box interior, HAlign halign, VAlign valign)
      : extents_(std::move(extents)),
        x_offset_(halign.GetOffset(extents_, interior)),
        y_offset_(valign.GetOffset(extents_, interior)),
        interior_(Box::intersect(interior.translate(x_offset_, y_offset_),
                                 extents_)) {}

  const Box &extents() const { return extents_; }
  int16_t x_offset() const { return x_offset_; }
  int16_t y_offset() const { return y_offset_; }
  const Box &interior() const { return interior_; }

 private:
  // Absolute extents, when the border is drawn at (0, 0).
  Box extents_;

  // Absolute offset by which the interior needs to be shifted horizontally in
  // order to comply with requested alignment.
  int16_t x_offset_;

  // Absolute offset by which the interior needs to be shifted vertically in
  // order to comply with requested alignment.
  int16_t y_offset_;

  // Absolute coordinates of the interior, when the border is drawn at (0, 0).
  Box interior_;
};

class TileBase : public Drawable {
 public:
  TileBase(const Drawable &interior, Box extents, HAlign halign, VAlign valign,
           Color bgcolor)
      : border_(std::move(extents), std::move(interior.extents()), halign,
                valign),
        bgcolor_(bgcolor) {}

  Color bgcolor() const { return bgcolor_; }

  void setBgColor(Color bgcolor) { bgcolor_ = bgcolor; }

  // template <typename... Args>
  // TileOf(Args &&... args, Rect extents, int16_t x_offset, int16_t y_offset)
  //     : interior_(std::forward<Args>(args)...),
  //       border_(std::move(extents),
  //               interior_.extents().translate(x_offset, y_offset)) {}

  Box extents() const override { return border_.extents(); }

 protected:
  void draw(const Surface &s, const Drawable &interior) const;

 private:
  internal::SolidBorder border_;
  Color bgcolor_;
};

}  // namespace internal

// Tile is a rectangular drawable with a single-colored background and an
// arbitrary interior (represented by a separate drawable). Tile is useful
// for drawing UI widgets, such as icons, text boxes, etc., which occupy
// rectangluar spaces. Tile handles auto-alignment and flickerless
// redraws.
//
// Alignment: the Tile re-posiions the interior within its bounding rectangle,
// according to the alignment specification, separately for horizontal and
// vertical axes ({ TOP, MIDDLE, BOTTOM }, { LEFT, CENTER, RIGHT }), with
// optional absolute offsets. The original absolute extents of the interior
// are ignored in that case; only the width and height of the interior are
// affecting its position within the tile. Alternatively, you can also disable
// re-positioning, and use the interior's absolute extents.
//
// Flickerless redraws: A naive way to draw the tile would be to fill-rect the
// background, and then draw the interior. That, however, would draw certain
// pixels twice, causing flicker and slowness. To minimize or avoid this effect,
// the tile is drawn as (up to) 5 non-overlapping sections: top border, left
// border, interior, right border, bottom border. If the interior itself
// supports no-flicker redraws, the tile is redrawn completely without flicker.
//
// The Tile can be created with a (semi)transparent background. In this case,
// bgcolor provided when the tile is drawn will dictate the effective
// background color of the tile: the effective background color will be
// the tile's bgcolor alpha-blended over the bgcolor specified at draw time.
// This feature is useful e.g. to visually differentiate state of UI widgets,
// e.g. active, inactive, selected, clicked, etc.
class Tile : public internal::TileBase {
 public:
  // Creates a tile with the specified interior, alignment, and optionally
  // background color.
  Tile(const Drawable *interior, Box extents, HAlign halign, VAlign valign,
       Color bgcolor = color::Transparent)
      : internal::TileBase(*interior, extents, halign, valign, bgcolor),
        interior_(interior) {}

  void drawTo(const Surface &s) const override {
    TileBase::draw(s, *interior_);
  }

 private:
  const Drawable *interior_;
};

// Similar to Tile, but rather than requiring the interior to be defined
// as a separate object, allows for it to be embedded in this one. Useful e.g.
// when you need to create and return a tile, which thus cannot refer to
// another non-owned object.
template <typename DrawableType>
class TileOf : public internal::TileBase {
 public:
  // Creates a tile with the specified interior, alignment, and optionally
  // background color.
  TileOf(DrawableType interior, Box extents, HAlign halign, VAlign valign,
         Color bgcolor = color::Transparent)
      : internal::TileBase(interior, extents, halign, valign, bgcolor),
        interior_(std::move(interior)) {}

  // template <typename... Args>
  // TileOf(Rect extents, HAlign halign, VAlign valign, Args &&... args)
  //     : internal::TileBase(interior, extents, halign, valign),
  //       interior_(std::forward<Args>(args)...),
  //       border_(std::move(extents),
  //               interior_.extents().translate(x_offset, y_offset)) {}

  void drawTo(const Surface &s) const override { TileBase::draw(s, interior_); }

 private:
  DrawableType interior_;
};

// Convenience function that creates a tile with a specified interior.
template <typename DrawableType, typename... Args>
TileOf<DrawableType> MakeTileOf(DrawableType interior, Args &&... args) {
  return TileOf<DrawableType>(std::move(interior), std::forward<Args>(args)...);
}

}  // namespace roo_display
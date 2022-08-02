#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/filter/background.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

namespace internal {

class SolidBorder {
 public:
  SolidBorder(Box extents, Box interior, Box anchorExtents, HAlign halign,
              VAlign valign, int16_t dx, int16_t dy)
      : extents_(std::move(extents)),
        x_offset_(halign.GetOffset(extents_, anchorExtents) + dx),
        y_offset_(valign.GetOffset(extents_, anchorExtents) + dy),
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
  TileBase(const Drawable &interior, Box extents, Alignment alignment,
           int16_t dx, int16_t dy, Color bgcolor,
           FillMode fill_mode = FILL_MODE_RECTANGLE)
      : border_(std::move(extents), std::move(interior.extents()),
                std::move(interior.anchorExtents()), alignment.h(),
                alignment.v(), dx, dy),
        bgcolor_(bgcolor),
        background_(nullptr),
        fill_mode_(fill_mode) {}

  TileBase(const Drawable &interior, Box extents, Alignment alignment,
           int16_t dx, int16_t dy, const Rasterizable *background,
           FillMode fill_mode = FILL_MODE_RECTANGLE)
      : border_(std::move(extents), std::move(interior.extents()),
                std::move(interior.anchorExtents()), alignment.h(),
                alignment.v(), dx, dy),
        bgcolor_(color::Transparent),
        background_(background),
        fill_mode_(fill_mode) {}

  FillMode fillMode() const { return fill_mode_; }

  void setBackground(Color bgcolor) {
    bgcolor_ = bgcolor;
    background_ = nullptr;
  }

  void setBackground(const Rasterizable *background) {
    bgcolor_ = color::Transparent;
    background_ = background;
  }

  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  // template <typename... Args>
  // TileOf(Args &&... args, Rect extents, int16_t x_offset, int16_t y_offset)
  //     : interior_(std::forward<Args>(args)...),
  //       border_(std::move(extents),
  //               interior_.extents().translate(x_offset, y_offset)) {}

  Box extents() const override { return border_.extents(); }

 protected:
  void draw(const Surface &s, const Drawable &interior) const;
  void drawInternal(const Surface &s, const Drawable &interior) const;

 private:
  internal::SolidBorder border_;
  Color bgcolor_;
  const Rasterizable *background_;
  FillMode fill_mode_;
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
  // Utility function to calculate the bounds of the tile's interior, given
  // its exterior bounds, the original interior bounds, and the alignment
  // constraints. The function returns the same bounds that the interior will
  // have when a tile with these pararameters is drawn.
  static Box InteriorBounds(const Box &exterior, const Box &interior,
                            Alignment alignment, int16_t dx = 0,
                            int16_t dy = 0) {
    return interior.translate(alignment.h().GetOffset(exterior, interior) + dx,
                              alignment.v().GetOffset(exterior, interior) + dy);
  }

  // Creates a tile with the specified interior, alignment, and optionally
  // background color.
  Tile(const Drawable *interior, Box extents, Alignment alignment,
       Color bgcolor = color::Transparent,
       FillMode fill_mode = FILL_MODE_RECTANGLE)
      : internal::TileBase(*interior, extents, alignment, 0, 0, bgcolor,
                           fill_mode),
        interior_(interior) {}

  // Creates a tile with the specified interior, alignment, interior offset,
  // and optionally background color.
  Tile(const Drawable *interior, Box extents, Alignment alignment, int16_t dx,
       int16_t dy, Color bgcolor = color::Transparent,
       FillMode fill_mode = FILL_MODE_RECTANGLE)
      : internal::TileBase(*interior, extents, alignment, dx, dy, bgcolor,
                           fill_mode),
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
  TileOf(DrawableType interior, Box extents, Alignment alignment,
         Color bgcolor = color::Transparent,
         FillMode fill_mode = FILL_MODE_RECTANGLE)
      : internal::TileBase(interior, extents, alignment, 0, 0, bgcolor,
                           fill_mode),
        interior_(std::move(interior)) {}

  TileOf(DrawableType interior, Box extents, Alignment alignment, int16_t dx,
         int16_t dy, Color bgcolor = color::Transparent,
         FillMode fill_mode = FILL_MODE_RECTANGLE)
      : internal::TileBase(interior, extents, alignment, dx, dy, bgcolor,
                           fill_mode),
        interior_(std::move(interior)) {}

  TileOf(DrawableType interior, Box extents, Alignment alignment,
         const Rasterizable *background,
         FillMode fill_mode = FILL_MODE_RECTANGLE)
      : internal::TileBase(interior, extents, alignment, 0, 0, background,
                           fill_mode),
        interior_(std::move(interior)) {}

  TileOf(DrawableType interior, Box extents, Alignment alignment, int16_t dx,
         int16_t dy, const Rasterizable *background,
         FillMode fill_mode = FILL_MODE_RECTANGLE)
      : internal::TileBase(interior, extents, alignment, alignment, dx, dy,
                           background, fill_mode),
        interior_(std::move(interior)) {}

  // template <typename... Args>
  // TileOf(Rect extents, HAlign halignment,VAlign valignment,Args &&... args)
  //     : internal::TileBase(interior, extents, halignment,valign),
  //       interior_(std::forward<Args>(args)...),
  //       border_(std::move(extents),
  //               interior_.extents().translate(x_offset, y_offset)) {}

  void drawTo(const Surface &s) const override { TileBase::draw(s, interior_); }

 private:
  DrawableType interior_;
};

// Convenience function that creates a tile with a specified interior and an
// optional alignment.
template <typename DrawableType>
TileOf<DrawableType> MakeTileOf(DrawableType interior, Box extents,
                                Alignment alignment = kNoAlign,
                                Color bgcolor = color::Transparent,
                                FillMode fill_mode = FILL_MODE_RECTANGLE) {
  return TileOf<DrawableType>(std::move(interior), std::move(extents),
                              alignment, bgcolor, fill_mode);
}

// Convenience function that creates a tile with a specified interior,
// alignment, and an absolute interior offset.
template <typename DrawableType>
TileOf<DrawableType> MakeTileOf(DrawableType interior, Box extents,
                                Alignment alignment, int16_t dx, int16_t dy,
                                Color bgcolor = color::Transparent,
                                FillMode fill_mode = FILL_MODE_RECTANGLE) {
  return TileOf<DrawableType>(std::move(interior), std::move(extents),
                              alignment, dx, dy, bgcolor, fill_mode);
}

// Similar to the above, but takes an arbitrary background.
template <typename DrawableType>
TileOf<DrawableType> MakeTileOf(DrawableType interior, Box extents,
                                Alignment alignment,
                                const Rasterizable *background,
                                FillMode fill_mode = FILL_MODE_RECTANGLE) {
  return TileOf<DrawableType>(std::move(interior), std::move(extents),
                              alignment, background, fill_mode);
}

// Similar to the above, but takes an arbitrary background.
template <typename DrawableType>
TileOf<DrawableType> MakeTileOf(DrawableType interior, Box extents,
                                Alignment alignment, int16_t dx, int16_t dy,
                                const Rasterizable *background,
                                FillMode fill_mode = FILL_MODE_RECTANGLE) {
  return TileOf<DrawableType>(std::move(interior), std::move(extents),
                              alignment, dx, dy, background, fill_mode);
}

}  // namespace roo_display